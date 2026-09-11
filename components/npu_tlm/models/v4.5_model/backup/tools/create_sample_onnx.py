#!/usr/bin/env python3
"""
Generates a sample ONNX model (sample_vit_block.onnx) for testing the ONNX compiler.
"""
import onnx
from onnx import helper, TensorProto

def create_sample_vit_onnx(filename="sample_vit_block.onnx"):
    # Define Inputs & Outputs
    X = helper.make_tensor_value_info('input_x', TensorProto.FLOAT, [64, 64])
    Y = helper.make_tensor_value_info('output_y', TensorProto.FLOAT, [64, 64])

    # Initializers (Weights & Biases)
    w_qkv = helper.make_tensor('w_qkv', TensorProto.FLOAT, [64, 192], [0.01]*64*192)
    gamma = helper.make_tensor('gamma', TensorProto.FLOAT, [64], [1.0]*64)
    beta = helper.make_tensor('beta', TensorProto.FLOAT, [64], [0.0]*64)
    w_proj = helper.make_tensor('w_proj', TensorProto.FLOAT, [64, 64], [0.02]*64*64)

    # Nodes
    node1 = helper.make_node('LayerNormalization', ['input_x', 'gamma', 'beta'], ['x_ln'], name='LN1')
    node2 = helper.make_node('Gemm', ['x_ln', 'w_proj'], ['proj_out'], name='Proj_Gemm')
    node3 = helper.make_node('Softmax', ['proj_out'], ['attn_out'], name='Attn_Softmax')
    node4 = helper.make_node('Add', ['attn_out', 'input_x'], ['output_y'], name='Residual_Add')

    # Graph
    graph = helper.make_graph(
        [node1, node2, node3, node4],
        'sample_vit_graph',
        [X],
        [Y],
        initializer=[w_qkv, gamma, beta, w_proj]
    )

    # Model
    model = helper.make_model(graph, producer_name='sauria_onnx_gen')
    onnx.save(model, filename)
    print(f"[SAMPLE GEN] Saved sample ONNX model: {filename}")

if __name__ == "__main__":
    create_sample_vit_onnx()
