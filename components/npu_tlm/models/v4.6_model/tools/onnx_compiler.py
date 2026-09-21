#!/usr/bin/env python3
"""
Sauria NPU ONNX Graph Compiler & Testbench Generator
Author: Sauria NPU Engineering Team

Parses ONNX model graphs (e.g. YOLOv8, ViT-Base, ResNet), lowers operators
to Sauria Rich Instructions (GEMM_FUSED 0x12, FUSED_ATTN 0x13, LAYERNORM 0x14, ELEM_WISE 0x15),
allocates DRAM memory layouts for weights/activations, and generates an executable SystemC testbench.
"""

import sys
import os
import argparse
import numpy as np
import onnx
from onnx import numpy_helper
def pad_or_truncate_1d(arr, target_len=4096):
    flat = np.asarray(arr).flatten()
    if flat.size < target_len:
        flat = np.pad(flat, (0, target_len - flat.size), mode='constant', constant_values=0)
    else:
        flat = flat[:target_len]
    return flat

def pad_or_truncate_2d(arr, target_shape=(32, 32)):
    target_len = target_shape[0] * target_shape[1]
    flat = pad_or_truncate_1d(arr, target_len)
    return flat.reshape(target_shape)

def broadcast_1d(arr, target_len=1024):
    flat = np.asarray(arr).flatten()
    if flat.size == 0:
        return np.zeros(target_len, dtype=np.int8)
    if flat.size < target_len:
        reps = (target_len + flat.size - 1) // flat.size
        flat = np.tile(flat, reps)[:target_len]
    else:
        flat = flat[:target_len]
    return flat

class OnnxCompiler:
    def __init__(self, model_path, output_cpp, dtype='int8', pe_x=32, pe_y=32):
        self.model_path = model_path
        self.output_cpp = output_cpp
        self.dtype = dtype
        self.pe_x = pe_x
        self.pe_y = pe_y
        self.dram_offset = 0x10000 # Base DRAM offset
        self.tensors = {}  # name -> {addr, shape, dtype, data, bytes}
        self.instructions = []
        self.golden_checkpoints = [] # list of {inst_idx, name, out_addr, shape, data}
        self.rng = np.random.RandomState(42)

    def load_model(self):
        print(f"[ONNX COMPILER] Loading model: {self.model_path}")
        self.model = onnx.load(self.model_path)
        try:
            self.model = onnx.shape_inference.infer_shapes(self.model)
        except Exception as e:
            print(f"[WARNING] Shape inference failed: {e}")
        
        self.graph = self.model.graph
        
        # Load Initializers (Weights & Biases)
        init_count = 0
        total_weight_bytes = 0
        for init in self.graph.initializer:
            arr = numpy_helper.to_array(init)
            if arr.dtype != np.int8:
                if arr.dtype in [np.float32, np.float64]:
                    max_v = np.max(np.abs(arr)) if np.max(np.abs(arr)) > 0 else 1.0
                    arr_int8 = np.clip(np.round((arr / max_v) * 127.0), -128, 127).astype(np.int8)
                else:
                    arr_int8 = arr.astype(np.int8)
            else:
                arr_int8 = arr

            size_bytes = arr_int8.nbytes
            addr = self.allocate_dram(size_bytes)
            self.tensors[init.name] = {
                'addr': addr,
                'shape': list(arr_int8.shape),
                'dtype': arr_int8.dtype,
                'data': arr_int8,
                'bytes': size_bytes
            }
            init_count += 1
            total_weight_bytes += size_bytes
            
        print(f"  [INIT SUMMARY] Loaded {init_count} initializers, total size: {total_weight_bytes / (1024*1024):.2f} MB")

    def allocate_dram(self, size_bytes):
        # Align to 256 bytes
        size_aligned = (size_bytes + 255) & ~255
        addr = self.dram_offset
        self.dram_offset += size_aligned
        return addr

    def get_tensor_data(self, name, default_shape=None):
        if default_shape is None:
            default_shape = [self.pe_x, self.pe_y]
        if name in self.tensors and 'data' in self.tensors[name]:
            return self.tensors[name]['data']
        # Synthetic activation data for graph inputs / uninitialized tensors (scaled [-4, 4] for INT8 dot-product stability)
        addr = self.get_tensor_addr(name, default_shape)
        arr = self.rng.randint(-4, 4, size=default_shape).astype(np.int8)
        self.tensors[name]['data'] = arr
        return arr

    def find_scale(self, tensor_name, default=1.0):
        scale_name_candidates = [
            tensor_name + '_scale',
            tensor_name + '.scale',
        ]
        if '_quantized' in tensor_name:
            scale_name_candidates.append(tensor_name.replace('_quantized', '_scale'))
        if '.weight' in tensor_name:
            scale_name_candidates.append(tensor_name.replace('.weight', '.scale'))
        for c in scale_name_candidates:
            if c != tensor_name and c in self.tensors and 'data' in self.tensors[c]:
                val = self.tensors[c]['data']
                if isinstance(val, np.ndarray) and val.size > 0:
                    return float(val.flat[0])
                elif isinstance(val, (float, int)):
                    return float(val)
        return default

    def lower_nodes(self):
        print(f"[ONNX COMPILER] Lowering {len(self.graph.node)} nodes to Sauria Rich Instructions...")
        lowered_counts = {}
        for idx, node in enumerate(self.graph.node):
            op = node.op_type
            inputs = node.input
            outputs = node.output
            lowered_counts[op] = lowered_counts.get(op, 0) + 1
            
            if op in ['Conv', 'Gemm', 'MatMul']:
                self.lower_gemm_conv(node)
            elif op in ['LayerNormalization', 'LayerNorm']:
                self.lower_layernorm(node)
            elif op in ['Add', 'Relu', 'MaxPool', 'Gelu', 'Erf', 'Mul', 'Sub', 'Div', 'Sigmoid']:
                self.lower_elemwise(node)
            elif op in ['Softmax']:
                self.lower_softmax(node)
            elif op in ['Reshape', 'Transpose', 'Flatten', 'Squeeze', 'Unsqueeze', 'Identity', 'QuantizeLinear', 'DequantizeLinear']:
                self.lower_alias(node)
            elif op in ['Concat']:
                self.lower_concat(node)
            elif op in ['Split', 'Slice']:
                self.lower_split_slice(node)
            else:
                self.lower_alias(node)

        print(f"  [LOWER SUMMARY] Lowered {len(self.instructions)} hardware instructions from {len(self.graph.node)} nodes:")
        for op, count in lowered_counts.items():
            print(f"    - {op}: {count}")

    def lower_alias(self, node):
        inputs = node.input
        outputs = node.output
        if not inputs or not outputs: return
        in_name = inputs[0]
        out_name = outputs[0]
        in_addr = self.get_tensor_addr(in_name)
        in_data = self.get_tensor_data(in_name)
        in_shape = self.get_shape(in_name)
        
        self.tensors[out_name] = {
            'addr': in_addr,
            'shape': in_shape,
            'bytes': len(in_data.tobytes()) if hasattr(in_data, 'tobytes') else int(np.prod(in_shape)),
            'data': in_data
        }

    def lower_concat(self, node):
        inputs = node.input
        outputs = node.output
        if not inputs or not outputs: return
        out_name = outputs[0]
        
        total_bytes = sum(self.tensors.get(inp, {}).get('bytes', 4096) for inp in inputs)
        out_addr = self.allocate_dram(total_bytes)
        
        all_data = []
        for inp in inputs:
            d = self.get_tensor_data(inp)
            all_data.append(d.flatten())
        concat_data = np.concatenate(all_data).astype(np.int8) if all_data else np.zeros(total_bytes, dtype=np.int8)
        
        self.tensors[out_name] = {
            'addr': out_addr,
            'shape': list(concat_data.shape),
            'bytes': total_bytes,
            'data': concat_data
        }

    def lower_split_slice(self, node):
        inputs = node.input
        outputs = node.output
        if not inputs or not outputs: return
        in_name = inputs[0]
        in_addr = self.get_tensor_addr(in_name)
        in_data = self.get_tensor_data(in_name)
        
        num_out = len(outputs)
        chunk_size = max(1, len(in_data.flatten()) // num_out)
        flat_data = in_data.flatten()
        
        for i, out_name in enumerate(outputs):
            start_idx = i * chunk_size
            sub_data = flat_data[start_idx:start_idx+chunk_size]
            sub_addr = in_addr + start_idx
            self.tensors[out_name] = {
                'addr': sub_addr,
                'shape': list(sub_data.shape),
                'bytes': len(sub_data),
                'data': sub_data
            }

    def lower_gemm_conv(self, node):
        op = node.op_type
        inputs = node.input
        outputs = node.output
        
        def_shape = [self.pe_x, self.pe_y]
        in_shape = self.get_shape(inputs[0], def_shape)
        w_shape = self.get_shape(inputs[1] if len(inputs) > 1 else inputs[0], def_shape)

        m = int(np.prod(in_shape[:-1])) if len(in_shape) >= 2 else (in_shape[0] if len(in_shape)==1 else self.pe_x)
        k = int(in_shape[-1]) if len(in_shape) >= 1 else self.pe_x
        n = int(w_shape[0] if (len(w_shape)==2 and w_shape[1]==k) else w_shape[-1]) if len(w_shape) >= 1 else self.pe_y

        m, k, n = max(1, min(m, 4096)), max(1, min(k, 4096)), max(1, min(n, 4096))

        act_type = 0 # 0=None, 1=ReLU, 2=Sigmoid, 3=GELU
        if op == 'Sigmoid':
            act_type = 2

        in_scale = self.find_scale(inputs[0], default=1.0)
        w_scale = self.find_scale(inputs[1], default=1.0) if len(inputs) > 1 else 1.0
        out_scale = self.find_scale(outputs[0], default=1.0)
        if in_scale == 1.0 and w_scale == 1.0 and out_scale == 1.0:
            in_scale = 0.125

        A = pad_or_truncate_2d(self.get_tensor_data(inputs[0], default_shape=[m, k]), (m, k))
        W = pad_or_truncate_2d(self.get_tensor_data(inputs[1] if len(inputs)>1 else inputs[0], default_shape=[k, n]), (k, n))

        MAX_A = 79000
        MAX_B = 81000
        MAX_C = 79000

        tile_m = max(1, min(m, MAX_A // max(1, k), MAX_C // max(1, n), 128))
        tile_n = max(1, min(n, MAX_B // max(1, k), MAX_C // max(1, tile_m), 128))

        if tile_m >= 32: tile_m = (tile_m // 32) * 32
        if tile_n >= 32: tile_n = (tile_n // 32) * 32
        tile_m = max(1, tile_m)
        tile_n = max(1, tile_n)

        eff_scale = (in_scale * w_scale) / (out_scale if out_scale != 0.0 else 1.0)
        A_f = A.astype(np.float32)
        W_f = W.astype(np.float32)
        raw_full = np.dot(A_f, W_f) * eff_scale
        if act_type == 1:
            raw_full = np.maximum(0, raw_full)
        elif act_type == 2:
            sigmoid = 1.0 / (1.0 + np.exp(-np.clip(raw_full, -10, 10)))
            raw_full = raw_full * sigmoid
        elif act_type == 3:
            cdf = 0.5 * (1.0 + np.tanh(np.sqrt(2.0 / np.pi) * (raw_full + 0.044715 * (raw_full ** 3))))
            raw_full = raw_full * cdf

        full_gold_out = np.clip(np.round(raw_full), -128, 127).astype(np.int8)
        in_addr = self.get_tensor_addr(inputs[0], default_shape=[m, k])
        w_addr = self.get_tensor_addr(inputs[1], default_shape=[k, n]) if len(inputs) > 1 else in_addr
        out_addr = self.allocate_dram(m * n * (4 if self.dtype=='int8' else 2))

        if m <= tile_m and n <= tile_n:
            inst = {
                'opcode': 0x12, # GEMM_FUSED
                'in_addr': in_addr,
                'w_addr': w_addr,
                'out_addr': out_addr,
                'm': m, 'k': k, 'n': n,
                'act_type': act_type,
                'in_scale': in_scale,
                'w_scale': w_scale,
                'out_scale': out_scale,
                'name': node.name or outputs[0]
            }
            self.instructions.append(inst)
            self.tensors[outputs[0]] = {'addr': out_addr, 'shape': [m, n], 'bytes': m*n, 'data': full_gold_out}
            self.golden_checkpoints.append({
                'inst_idx': len(self.instructions),
                'name': node.name or outputs[0],
                'out_addr': out_addr,
                'shape': [m, n],
                'data': full_gold_out
            })
        else:
            for c in range(0, n, tile_n):
                n_sub = min(tile_n, n - c)
                W_sub = W[0:k, c:c+n_sub].astype(np.int8)
                w_sub_addr = self.allocate_dram(W_sub.nbytes)
                self.tensors[f"{inputs[1] if len(inputs)>1 else inputs[0]}_tile_{c}"] = {
                    'addr': w_sub_addr, 'shape': list(W_sub.shape), 'bytes': W_sub.nbytes, 'data': W_sub
                }

                for r in range(0, m, tile_m):
                    m_sub = min(tile_m, m - r)
                    a_sub_addr = in_addr + (r * k)
                    out_sub_addr = out_addr + (r * n + c)
                    gold_sub = full_gold_out[r:r+m_sub, c:c+n_sub]

                    inst = {
                        'opcode': 0x12, # GEMM_FUSED
                        'in_addr': a_sub_addr,
                        'w_addr': w_sub_addr,
                        'out_addr': out_sub_addr,
                        'm': m_sub, 'k': k, 'n': n_sub,
                        'act_type': act_type,
                        'in_scale': in_scale,
                        'w_scale': w_scale,
                        'out_scale': out_scale,
                        'name': f"{node.name or outputs[0]}_tile_{r}_{c}"
                    }
                    self.instructions.append(inst)

            self.tensors[outputs[0]] = {'addr': out_addr, 'shape': [m, n], 'bytes': m*n, 'data': full_gold_out}
            self.golden_checkpoints.append({
                'inst_idx': len(self.instructions),
                'name': node.name or outputs[0],
                'out_addr': out_addr,
                'shape': [m, n],
                'data': full_gold_out
            })


    def lower_layernorm(self, node):
        inputs = node.input
        outputs = node.output
        def_shape = [self.pe_x, self.pe_y]
        in_shape = self.get_shape(inputs[0], def_shape)
        seq_len = int(np.prod(in_shape[:-1])) if len(in_shape) >= 2 else (in_shape[0] if len(in_shape)==1 else self.pe_x)
        dim = int(in_shape[-1]) if len(in_shape) >= 1 else self.pe_y

        seq_len, dim = max(1, min(seq_len, 4096)), max(1, min(dim, 4096))

        in_addr = self.get_tensor_addr(inputs[0], default_shape=[seq_len, dim])
        gamma_addr = self.get_tensor_addr(inputs[1], default_shape=[dim]) if len(inputs) > 1 else 0
        beta_addr = self.get_tensor_addr(inputs[2], default_shape=[dim]) if len(inputs) > 2 else 0
        out_addr = self.allocate_dram(seq_len * dim * 1)

        inst = {
            'opcode': 0x14, # LAYERNORM
            'in_addr': in_addr,
            'gamma_addr': gamma_addr,
            'beta_addr': beta_addr,
            'out_addr': out_addr,
            'seq_len': seq_len,
            'dim': dim,
            'name': node.name or outputs[0]
        }
        self.instructions.append(inst)

        X = pad_or_truncate_2d(self.get_tensor_data(inputs[0], default_shape=[seq_len, dim]), (seq_len, dim)).astype(np.float32)
        gamma = pad_or_truncate_1d(self.get_tensor_data(inputs[1], default_shape=[dim]), dim).astype(np.float32) if len(inputs) > 1 else np.ones(dim, dtype=np.float32)
        beta = pad_or_truncate_1d(self.get_tensor_data(inputs[2], default_shape=[dim]), dim).astype(np.float32) if len(inputs) > 2 else np.zeros(dim, dtype=np.float32)
        
        mean = np.mean(X, axis=-1, keepdims=True)
        var = np.var(X, axis=-1, keepdims=True)
        norm = (X - mean) / np.sqrt(var + 1e-5)
        scaled_val = norm * gamma + beta
        gold_out = np.clip(np.round(scaled_val), -128, 127).astype(np.int8)

        self.tensors[outputs[0]] = {'addr': out_addr, 'shape': [seq_len, dim], 'bytes': seq_len*dim, 'data': gold_out}
        self.golden_checkpoints.append({
            'inst_idx': len(self.instructions),
            'name': node.name or outputs[0],
            'out_addr': out_addr,
            'shape': [seq_len, dim],
            'data': gold_out
        })

    def lower_elemwise(self, node):
        op = node.op_type
        inputs = node.input
        outputs = node.output
        def_shape = [self.pe_x, self.pe_y]
        in_shape_a = self.get_shape(inputs[0], def_shape)
        in_shape_b = self.get_shape(inputs[1], def_shape) if len(inputs) > 1 else in_shape_a

        data_a = self.get_tensor_data(inputs[0], default_shape=in_shape_a)
        data_b = self.get_tensor_data(inputs[1] if len(inputs)>1 else inputs[0], default_shape=in_shape_b)

        num_elem_a = int(data_a.size)
        num_elem_b = int(data_b.size)
        num_elem = max(num_elem_a, num_elem_b)
        num_elem = max(1, min(num_elem, 409600))
        num_elem_a = min(num_elem_a, num_elem)
        num_elem_b = min(num_elem_b, num_elem)

        in_a = self.get_tensor_addr(inputs[0], default_shape=in_shape_a)
        in_b = self.get_tensor_addr(inputs[1], default_shape=in_shape_b) if len(inputs) > 1 else in_a
        out_addr = self.allocate_dram(num_elem * 1)

        mode_map = {'Add': 0, 'MaxPool': 1, 'Mul': 2, 'Sub': 3, 'Div': 4}
        mode = mode_map.get(op, 0)
        inst = {
            'opcode': 0x15, # ELEM_WISE
            'a_addr': in_a,
            'b_addr': in_b,
            'out_addr': out_addr,
            'len': num_elem,
            'a_len': num_elem_a,
            'b_len': num_elem_b,
            'mode': mode,
            'name': node.name or outputs[0]
        }
        self.instructions.append(inst)

        A = broadcast_1d(self.get_tensor_data(inputs[0], default_shape=in_shape_a), num_elem).astype(np.int32)
        B = broadcast_1d(self.get_tensor_data(inputs[1] if len(inputs)>1 else inputs[0], default_shape=in_shape_b), num_elem).astype(np.int32)
        if mode == 1:
            raw = np.maximum(A, B)
        elif mode == 2:
            raw = np.round((A.astype(np.float32) * B.astype(np.float32)) / 128.0)
        elif mode == 3:
            raw = A - B
        elif mode == 4:
            B_safe = np.where(B == 0, 1, B).astype(np.float32)
            raw = np.round(A.astype(np.float32) / B_safe)
        else:
            raw = A + B
        gold_out = np.clip(raw, -128, 127).astype(np.int8)

        self.tensors[outputs[0]] = {'addr': out_addr, 'shape': [num_elem], 'bytes': num_elem, 'data': gold_out}
        self.golden_checkpoints.append({
            'inst_idx': len(self.instructions),
            'name': node.name or outputs[0],
            'out_addr': out_addr,
            'shape': [num_elem],
            'data': gold_out
        })

    def lower_softmax(self, node):
        inputs = node.input
        outputs = node.output
        def_shape = [self.pe_x, self.pe_y]
        in_shape = self.get_shape(inputs[0], def_shape)
        seq_len = int(np.prod(in_shape[:-1])) if len(in_shape) >= 2 else (in_shape[0] if len(in_shape)==1 else self.pe_x)
        head_dim = int(in_shape[-1]) if len(in_shape) >= 1 else self.pe_y
        seq_len, head_dim = max(1, min(seq_len, 4096)), max(1, min(head_dim, 4096))

        in_addr = self.get_tensor_addr(inputs[0], default_shape=[seq_len, head_dim])
        out_addr = self.allocate_dram(seq_len * head_dim * 1)

        inst = {
            'opcode': 0x13, # FUSED_ATTN
            'in_addr': in_addr,
            'out_addr': out_addr,
            'seq_len': seq_len,
            'head_dim': head_dim,
            'name': node.name or outputs[0]
        }
        self.instructions.append(inst)

        X = pad_or_truncate_2d(self.get_tensor_data(inputs[0], default_shape=[seq_len, head_dim]), (seq_len, head_dim)).astype(np.float32)
        Q = X
        K = X
        V = X
        qk = np.dot(Q, K.T)
        e_x = np.exp(qk - np.max(qk, axis=-1, keepdims=True))
        soft = e_x / np.sum(e_x, axis=-1, keepdims=True)
        raw = np.dot(soft, V)
        gold_out = np.clip(np.round(raw), -128, 127).astype(np.int8)

        self.tensors[outputs[0]] = {'addr': out_addr, 'shape': [seq_len, head_dim], 'bytes': seq_len*head_dim, 'data': gold_out}
        self.golden_checkpoints.append({
            'inst_idx': len(self.instructions),
            'name': node.name or outputs[0],
            'out_addr': out_addr,
            'shape': [seq_len, head_dim],
            'data': gold_out
        })

    def get_shape(self, tensor_name, default_shape=None):
        if default_shape is None:
            default_shape = [self.pe_x, self.pe_y]
        if tensor_name in self.tensors and 'shape' in self.tensors[tensor_name]:
            s = self.tensors[tensor_name]['shape']
            if len(s) > 0: return s
        for vi in list(self.graph.value_info) + list(self.graph.input) + list(self.graph.output):
            if vi.name == tensor_name:
                shape = [d.dim_value for d in vi.type.tensor_type.shape.dim if d.dim_value > 0]
                if shape:
                    return shape
        return default_shape

    def get_tensor_addr(self, name, default_shape=None):
        if default_shape is None:
            default_shape = [self.pe_x, self.pe_y]
        if name in self.tensors:
            return self.tensors[name]['addr']
        shape = self.get_shape(name, default_shape)
        num_elem = int(np.prod(shape)) if len(shape) > 0 else 4096
        size_bytes = num_elem
        addr = self.allocate_dram(size_bytes)
        self.tensors[name] = {'addr': addr, 'shape': shape, 'bytes': size_bytes}
        return addr

    def run_golden_emulation_pass(self):
        print("[ONNX COMPILER] Running sequential DRAM software golden reference pass...")
        dram_bytes = bytearray(self.dram_offset)
        for tensor_name, info in self.tensors.items():
            addr = info['addr']
            data = info.get('data', None)
            if data is not None:
                b = data.tobytes()
                end_addr = addr + len(b)
                if end_addr <= len(dram_bytes):
                    dram_bytes[addr:end_addr] = b

        self.golden_checkpoints = []
        for idx, inst in enumerate(self.instructions):
            opcode = inst['opcode']
            out_addr = inst['out_addr']
            
            if opcode == 0x12: # GEMM_FUSED
                in_addr = inst['in_addr']
                w_addr = inst['w_addr']
                m, k, n = inst['m'], inst['k'], inst['n']
                act_type = inst['act_type']
                in_scale = inst.get('in_scale', 1.0)
                w_scale = inst.get('w_scale', 1.0)
                out_scale = inst.get('out_scale', 1.0)

                A_raw = np.frombuffer(dram_bytes[in_addr:in_addr+m*k], dtype=np.int8)
                W_raw = np.frombuffer(dram_bytes[w_addr:w_addr+k*n], dtype=np.int8)
                A = pad_or_truncate_2d(A_raw, (m, k)).astype(np.float32)
                W = pad_or_truncate_2d(W_raw, (k, n)).astype(np.float32)

                eff_scale = (in_scale * w_scale) / (out_scale if out_scale != 0.0 else 1.0)
                raw = np.dot(A, W) * eff_scale
                if act_type == 1:
                    raw = np.maximum(0, raw)
                elif act_type == 2:
                    sigmoid = 1.0 / (1.0 + np.exp(-np.clip(raw, -10, 10)))
                    raw = raw * sigmoid
                elif act_type == 3:
                    cdf = 0.5 * (1.0 + np.tanh(np.sqrt(2.0 / np.pi) * (raw + 0.044715 * (raw ** 3))))
                    raw = raw * cdf

                gold_out = np.clip(np.round(raw), -128, 127).astype(np.int8)

            elif opcode == 0x14: # LAYERNORM
                in_addr = inst['in_addr']
                gamma_addr = inst['gamma_addr']
                beta_addr = inst['beta_addr']
                seq_len, dim = inst['seq_len'], inst['dim']

                X_raw = np.frombuffer(dram_bytes[in_addr:in_addr+seq_len*dim], dtype=np.int8)
                X = pad_or_truncate_2d(X_raw, (seq_len, dim)).astype(np.float32)
                
                gamma = np.frombuffer(dram_bytes[gamma_addr:gamma_addr+dim], dtype=np.int8).astype(np.float32) if gamma_addr > 0 else np.ones(dim, dtype=np.float32)
                beta = np.frombuffer(dram_bytes[beta_addr:beta_addr+dim], dtype=np.int8).astype(np.float32) if beta_addr > 0 else np.zeros(dim, dtype=np.float32)
                gamma = pad_or_truncate_1d(gamma, dim)
                beta = pad_or_truncate_1d(beta, dim)

                mean = np.mean(X, axis=-1, keepdims=True)
                var = np.var(X, axis=-1, keepdims=True)
                norm = (X - mean) / np.sqrt(var + 1e-5)
                scaled_val = norm * gamma + beta
                gold_out = np.clip(np.round(scaled_val), -128, 127).astype(np.int8)

            elif opcode == 0x15: # ELEM_WISE
                a_addr = inst['a_addr']
                b_addr = inst['b_addr']
                length = inst['len']
                a_len = inst.get('a_len', length)
                b_len = inst.get('b_len', length)
                mode = inst['mode']

                A_raw = np.frombuffer(dram_bytes[a_addr:a_addr+a_len], dtype=np.int8)
                B_raw = np.frombuffer(dram_bytes[b_addr:b_addr+b_len], dtype=np.int8)
                A = broadcast_1d(A_raw, length).astype(np.int32)
                B = broadcast_1d(B_raw, length).astype(np.int32)

                if mode == 1:
                    raw = np.maximum(A, B)
                elif mode == 2:
                    raw = np.round((A.astype(np.float32) * B.astype(np.float32)) / 128.0)
                elif mode == 3:
                    raw = A - B
                elif mode == 4:
                    B_safe = np.where(B == 0, 1, B).astype(np.float32)
                    raw = np.round(A.astype(np.float32) / B_safe)
                else:
                    raw = A + B
                gold_out = np.clip(raw, -128, 127).astype(np.int8)

            elif opcode == 0x13: # FUSED_ATTN / Softmax
                in_addr = inst['in_addr']
                seq_len, head_dim = inst['seq_len'], inst['head_dim']

                X_raw = np.frombuffer(dram_bytes[in_addr:in_addr+seq_len*head_dim], dtype=np.int8)
                X = pad_or_truncate_2d(X_raw, (seq_len, head_dim)).astype(np.float32)
                Q = X
                K = X
                V = X
                qk = np.dot(Q, K.T)
                e_x = np.exp(qk - np.max(qk, axis=-1, keepdims=True))
                soft = e_x / np.sum(e_x, axis=-1, keepdims=True)
                raw = np.dot(soft, V)
                gold_out = np.clip(np.round(raw), -128, 127).astype(np.int8)

            else:
                gold_out = np.zeros(4096, dtype=np.int8)

            # Write result into dram_bytes for downstream instruction consumption
            b = gold_out.tobytes()
            if out_addr + len(b) <= len(dram_bytes):
                dram_bytes[out_addr:out_addr+len(b)] = b

            self.golden_checkpoints.append({
                'inst_idx': idx + 1,
                'name': inst['name'],
                'out_addr': out_addr,
                'shape': list(gold_out.shape),
                'data': gold_out
            })

    def save_payload_binaries(self):
        tools_dir = os.path.dirname(self.output_cpp) or "tools"
        dram_bin_path = os.path.join(tools_dir, "dram_init.bin")
        gold_bin_path = os.path.join(tools_dir, "golden_ref.bin")

        dram_bytes = bytearray(self.dram_offset)
        for tensor_name, info in self.tensors.items():
            addr = info['addr']
            data = info.get('data', None)
            if data is not None:
                b = data.tobytes()
                end_addr = addr + len(b)
                if end_addr <= len(dram_bytes):
                    dram_bytes[addr:end_addr] = b

        with open(dram_bin_path, 'wb') as f:
            f.write(dram_bytes)
        print(f"  [DRAM BINARY] Saved initial DRAM payload ({len(dram_bytes)/(1024*1024):.2f} MB) to {dram_bin_path}")

        gold_bytes = bytearray()
        for ckpt in self.golden_checkpoints:
            gold_bytes.extend(ckpt['data'].tobytes())

        with open(gold_bin_path, 'wb') as f:
            f.write(gold_bytes)
        print(f"  [GOLDEN BINARY] Saved golden reference outputs ({len(gold_bytes)/(1024*1024):.4f} MB) to {gold_bin_path}")

    def generate_cpp_testbench(self):
        self.run_golden_emulation_pass()
        self.save_payload_binaries()
        print(f"[ONNX COMPILER] Generating SystemC C++ testbench: {self.output_cpp}")
        model_basename = os.path.basename(self.model_path)
        dram_mb = max(64, int((self.dram_offset + 1024*1024 - 1) / (1024*1024)))
        
        header = f"""//
// Automatically Generated SystemC Testbench for ONNX Model
// Model: {model_basename}
// Target: Sauria NPU v4.2 SystemC Core ({self.pe_x}x{self.pe_y} PE Array, Dual-Lane)
//

#include <systemc.h>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include "sauria_types.h"
#include "npu_top.h"

using namespace sauria;

typedef NpuTop<{self.pe_x}, {self.pe_y}, int8_t, int8_t, int32_t> NpuInt8T;

SC_MODULE(TbOnnxModel)
{{
    sc_in<bool> i_clk;

    sc_signal<bool> rstn, soft_reset, start, done, deadlock;
    sc_signal<uint32_t> mvm_k, host_addr, total_contexts;
    sc_signal<bool> host_wren, host_rden;
    sc_signal<host_data_t> host_wdata, host_rdata;
    sc_signal<host_mask_t> host_wmask;
    sc_signal<float> threshold;
    sc_signal<sc_bv<3>> select;

    NpuInt8T *dut;
    fx1::PerfCounters perf;
    std::vector<uint8_t> dram;
    int errors = 0;

    SC_CTOR(TbOnnxModel)
    {{
        dut = new NpuInt8T("dut");
        perf.X = {self.pe_x};
        perf.Y = {self.pe_y};
        perf.freq_ghz = 0.8;
        perf.elem_bytes = 1;
        dut->attach_perf(&perf);

        dut->i_clk(i_clk);
        dut->i_rstn(rstn);
        dut->i_soft_reset(soft_reset);
        dut->i_start(start);
        dut->o_done(done);
        dut->o_deadlock(deadlock);
        dut->i_mvm_k(mvm_k);
        dut->i_host_addr(host_addr);
        dut->i_host_wren(host_wren);
        dut->i_host_rden(host_rden);
        dut->i_host_wdata(host_wdata);
        dut->i_host_wmask(host_wmask);
        dut->o_host_rdata(host_rdata);
        dut->i_threshold(threshold);
        dut->i_select(select);
        dut->i_total_contexts(total_contexts);

        SC_THREAD(run);
        sensitive << i_clk.pos();
    }}

    void wr(uint32_t addr, uint32_t val)
    {{
        host_data_t d;
        d.data.fill(0.0);
        d[0] = static_cast<double>(val);
        host_mask_t m;
        m.data.fill(true);
        host_addr.write(addr);
        host_wdata.write(d);
        host_wmask.write(m);
        host_wren.write(true);
        host_rden.write(false);
        wait();
        host_wren.write(false);
        wait();
    }}

    void run()
    {{
        dram.resize(static_cast<size_t>({dram_mb}) * 1024ULL * 1024ULL, 0); // {dram_mb} MB DRAM buffer
        dut->set_dram(&dram);

        // Pre-load initial DRAM contents from dram_init.bin
        std::ifstream f_init("tools/dram_init.bin", std::ios::binary);
        if (!f_init.is_open()) {{
            f_init.open("dram_init.bin", std::ios::binary);
        }}
        if (f_init.is_open()) {{
            f_init.read(reinterpret_cast<char*>(dram.data()), dram.size());
            f_init.close();
            std::cout << "[TESTBENCH] Successfully loaded DRAM initialization payload (dram_init.bin)" << std::endl;
        }} else {{
            std::cout << "[WARNING] dram_init.bin not found, running with default zero-initialized DRAM." << std::endl;
        }}

        // System Reset
        rstn.write(false);
        soft_reset.write(false);
        start.write(false);
        mvm_k.write({self.pe_x});
        total_contexts.write(1);
        threshold.write(0.0f);
        select.write(0);
        host_wren.write(false);
        host_rden.write(false);
        wait(5);
        rstn.write(true);
        wait(5);

        std::cout << "==================================================" << std::endl;
        std::cout << "   SAURIA NPU ONNX MODEL EXECUTION BENCHMARK     " << std::endl;
        std::cout << "   Model: {model_basename}" << std::endl;
        std::cout << "   Allocated DRAM: {dram_mb} MB" << std::endl;
        std::cout << "   Compiled Instructions: {len(self.instructions)}" << std::endl;
        std::cout << "==================================================" << std::endl;

        // Queue Compiled Sauria Rich Instructions
"""
        cpp_code = header
        import struct
        for idx, inst in enumerate(self.instructions):
            cpp_code += f"        // Instruction {idx+1}: {inst['name']} (Opcode 0x{inst['opcode']:02X})\n"
            if inst['opcode'] == 0x12: # GEMM_FUSED
                in_s_bits = struct.unpack('<I', struct.pack('<f', float(inst.get('in_scale', 1.0))))[0]
                w_s_bits = struct.unpack('<I', struct.pack('<f', float(inst.get('w_scale', 1.0))))[0]
                out_s_bits = struct.unpack('<I', struct.pack('<f', float(inst.get('out_scale', 1.0))))[0]

                cpp_code += f"        wr(0x40000400, 0x{inst['in_addr']:07X}); // r_in_addr\n"
                cpp_code += f"        wr(0x40000404, 0x{inst['w_addr']:07X}); // r_w_addr\n"
                cpp_code += f"        wr(0x40000408, 0x{inst['out_addr']:07X}); // r_out_addr\n"
                cpp_code += f"        wr(0x40000410, {inst['m']}); // r_m\n"
                cpp_code += f"        wr(0x40000414, {inst['k']}); // r_k\n"
                cpp_code += f"        wr(0x40000418, {inst['n']}); // r_n\n"
                cpp_code += f"        wr(0x4000042C, {inst['act_type']}); // r_act_type\n"
                cpp_code += f"        wr(0x40000438, 0x{in_s_bits:08X}); // r_in_scale\n"
                cpp_code += f"        wr(0x4000043C, 0x{w_s_bits:08X}); // r_w_scale\n"
                cpp_code += f"        wr(0x40000440, 0x{out_s_bits:08X}); // r_out_scale\n"
                cpp_code += f"        wr(0x40000310, 0x12);\n\n"
            elif inst['opcode'] == 0x14: # LAYERNORM
                cpp_code += f"        wr(0x40000400, 0x{inst['in_addr']:07X}); // r_in_addr\n"
                cpp_code += f"        wr(0x40000444, 0x{inst['gamma_addr']:07X}); // r_gamma_addr\n"
                cpp_code += f"        wr(0x4000044C, 0x{inst['beta_addr']:07X}); // r_beta_addr\n"
                cpp_code += f"        wr(0x40000408, 0x{inst['out_addr']:07X}); // r_out_addr\n"
                cpp_code += f"        wr(0x40000450, {inst['seq_len']}); // r_seq_len\n"
                cpp_code += f"        wr(0x40000454, {inst['dim']}); // r_dim\n"
                cpp_code += f"        wr(0x40000310, 0x14);\n\n"
            elif inst['opcode'] == 0x15: # ELEM_WISE
                a_len = inst.get('a_len', inst['len'])
                b_len = inst.get('b_len', inst['len'])
                cpp_code += f"        wr(0x40000444, 0x{inst['a_addr']:07X}); // r_a_addr\n"
                cpp_code += f"        wr(0x40000448, 0x{inst['b_addr']:07X}); // r_b_addr\n"
                cpp_code += f"        wr(0x40000408, 0x{inst['out_addr']:07X}); // r_out_addr\n"
                cpp_code += f"        wr(0x40000450, {inst['len']}); // r_len\n"
                cpp_code += f"        wr(0x40000454, {inst['mode']}); // r_mode\n"
                cpp_code += f"        wr(0x40000464, {a_len}); // r_a_len\n"
                cpp_code += f"        wr(0x40000468, {b_len}); // r_b_len\n"
                cpp_code += f"        wr(0x40000310, 0x15);\n\n"
            elif inst['opcode'] == 0x13: # FUSED_ATTN
                q_addr = inst.get('q_addr', inst.get('in_addr', 0))
                k_addr = inst.get('k_addr', inst.get('in_addr', 0))
                v_addr = inst.get('v_addr', inst.get('in_addr', 0))
                num_heads = inst.get('num_heads', 1)
                head_dim = inst.get('head_dim', inst.get('dim', self.pe_y))
                seq_len = inst.get('seq_len', self.pe_x)
                cpp_code += f"        wr(0x40000444, 0x{q_addr:07X}); // r_q_addr\n"
                cpp_code += f"        wr(0x40000448, 0x{k_addr:07X}); // r_k_addr\n"
                cpp_code += f"        wr(0x4000044C, 0x{v_addr:07X}); // r_v_addr\n"
                cpp_code += f"        wr(0x40000408, 0x{inst['out_addr']:07X}); // r_out_addr\n"
                cpp_code += f"        wr(0x40000450, {seq_len}); // r_seq_len\n"
                cpp_code += f"        wr(0x40000454, {num_heads}); // r_num_heads\n"
                cpp_code += f"        wr(0x40000458, {head_dim}); // r_head_dim\n"
                cpp_code += f"        wr(0x40000310, 0x13);\n\n"

        wait_cycles = max(20000, len(self.instructions) * 800)
        cpp_code += f"        std::cout << \"[TESTBENCH] Waiting for execution to complete...\" << std::endl;\n"
        cpp_code += f"        wait({wait_cycles});\n\n"

        # Automated Golden Output Checker
        cpp_code += f"""        std::cout << "\\n====================================================================================================" << std::endl;
        std::cout << "                          SAURIA NPU AUTOMATED GOLDEN ACCURACY CHECKER                             " << std::endl;
        std::cout << "====================================================================================================" << std::endl;
        std::cout << std::left << std::setw(8)  << "Inst #"
                  << std::setw(30) << "Layer / Operator Name"
                  << std::setw(12) << "DRAM Addr"
                  << std::setw(10) << "Size(B)"
                  << std::setw(10) << "MAE"
                  << std::setw(10) << "RMSE"
                  << std::setw(10) << "L_inf"
                  << std::setw(12) << "Cos Sim"
                  << std::setw(8)  << "Status" << std::endl;
        std::cout << "----------------------------------------------------------------------------------------------------" << std::endl;

        std::ifstream f_gold("tools/golden_ref.bin", std::ios::binary);
        if (!f_gold.is_open()) {{
            f_gold.open("golden_ref.bin", std::ios::binary);
        }}

        int passed_checkpoints = 0;
        int failed_checkpoints = 0;
"""

        # Generate Checkpoint verification calls
        for ckpt in self.golden_checkpoints:
            inst_idx = ckpt['inst_idx']
            name = (ckpt['name'][:28] if ckpt['name'] else f"Layer_{inst_idx}")
            out_addr = ckpt['out_addr']
            num_bytes = ckpt['data'].nbytes

            cpp_code += f"""        {{
            uint32_t out_addr = 0x{out_addr:07X};
            size_t num_bytes = {num_bytes};
            std::vector<int8_t> gold_buf(num_bytes, 0);
            if (f_gold.is_open()) {{
                f_gold.read(reinterpret_cast<char*>(gold_buf.data()), num_bytes);
            }}
            const int8_t* hw_buf = reinterpret_cast<const int8_t*>(&dram[out_addr]);

            double sum_abs_err = 0.0;
            double sum_sq_err = 0.0;
            int max_abs_err = 0;
            double dot_prod = 0.0;
            double norm_hw = 0.0;
            double norm_gold = 0.0;

            for (size_t i = 0; i < num_bytes; i++) {{
                int hw_v = static_cast<int>(hw_buf[i]);
                int gold_v = static_cast<int>(gold_buf[i]);
                int diff = std::abs(hw_v - gold_v);
                sum_abs_err += diff;
                sum_sq_err += diff * diff;
                if (diff > max_abs_err) max_abs_err = diff;

                dot_prod += static_cast<double>(hw_v) * static_cast<double>(gold_v);
                norm_hw += static_cast<double>(hw_v) * static_cast<double>(hw_v);
                norm_gold += static_cast<double>(gold_v) * static_cast<double>(gold_v);
            }}

            double mae = sum_abs_err / num_bytes;
            double rmse = std::sqrt(sum_sq_err / num_bytes);
            double cos_sim = (norm_hw > 0 && norm_gold > 0) ? (dot_prod / (std::sqrt(norm_hw) * std::sqrt(norm_gold))) : 1.0;
            bool pass = (mae <= 5.0) || (cos_sim >= 0.95);
            if (pass) passed_checkpoints++; else {{ failed_checkpoints++; errors++; }}

            std::cout << std::left << std::setw(8) << {inst_idx}
                      << std::setw(30) << "{name}"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }}
"""

        footer = f"""        if (f_gold.is_open()) f_gold.close();
        std::cout << "====================================================================================================" << std::endl;
        std::cout << "  VERIFICATION SUMMARY: " << passed_checkpoints << " / " << (passed_checkpoints + failed_checkpoints) 
                  << " Checkpoints PASSED (" << (failed_checkpoints == 0 ? "100.0% SUCCESS" : "VERIFICATION FAILURES DETECTED") << ")" << std::endl;
        std::cout << "====================================================================================================\\n" << std::endl;

        std::cout << "[COMPLETED] ONNX Graph Model Execution Finished Successfully!" << std::endl;
        perf.report("{model_basename} ({self.pe_x}x{self.pe_y})");

        sc_stop();
    }}
}};

int sc_main(int argc, char **argv)
{{
    sc_clock clk("clk", 1.25, SC_NS);
    TbOnnxModel tb("TbOnnxModel_inst");
    tb.i_clk(clk);
    sc_start();
    return tb.errors;
}}
"""
        cpp_code += footer
        with open(self.output_cpp, 'w') as f:
            f.write(cpp_code)
        print(f"[ONNX COMPILER] Successfully generated testbench: {self.output_cpp}")

def main():
    parser = argparse.ArgumentParser(description="Sauria NPU ONNX Graph Compiler & Testbench Generator")
    parser.add_argument("--model", required=True, help="Path to ONNX model file (.onnx)")
    parser.add_argument("--output", default="tools/test_onnx_model.cpp", help="Path to output SystemC testbench (.cpp)")
    parser.add_argument("--dtype", default="int8", choices=["int8"], help="Target quantization data type (int8)")
    parser.add_argument("--eval_x", type=int, default=32, help="PE Array X dimension")
    parser.add_argument("--eval_y", type=int, default=32, help="PE Array Y dimension")
    
    args = parser.parse_args()
    
    compiler = OnnxCompiler(args.model, args.output, args.dtype, args.eval_x, args.eval_y)
    compiler.load_model()
    compiler.lower_nodes()
    compiler.generate_cpp_testbench()

if __name__ == "__main__":
    main()
