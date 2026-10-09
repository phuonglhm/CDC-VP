# Tests của task

Thư mục dành cho unit test của module. Regression hiện gọi qua top-level trong
`integration_tests/tests/vp_tests.cpp`, gồm dma_32, dma_64, dma_128, completion, fault.
Khi thêm unit test, đăng ký executable/test tại CMakeLists.txt của task này.
