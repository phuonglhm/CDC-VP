# Tests của task

Thư mục dành cho unit test của module. Regression hiện gọi qua top-level trong
`integration_tests/tests/vp_tests.cpp`, gồm reset, registers.
Khi thêm unit test, đăng ký executable/test tại CMakeLists.txt của task này.
