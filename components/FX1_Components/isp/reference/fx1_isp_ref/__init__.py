"""FX1 ISP independent Python reference (milestone MR).

Frame-based, integer (numpy int64) implementation of the pipeline written from
the HAS/CSR contracts and the ALG decisions, independently of the C++ model.
It generates the expected output of the committed test vectors. It is NOT a
golden model from the IP owner (DEC-05): where the specification leaves a
choice, the decision ID it follows is cited in the code.
"""
