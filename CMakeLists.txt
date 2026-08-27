cmake_minimum_required(VERSION 3.21)

project(Distribuidora
    VERSION 0.1.0
    DESCRIPTION "ERP para distribuidora de bebidas (loja única) — Qt 6 + QML + SQLite"
    LANGUAGES CXX
)

# Qt 6 exige C++17 ou mais recente. Definido antes do find_package para que o
# CMake reclame cedo se o compilador for antigo demais.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2 Sql Network)

qt_standard_project_setup(REQUIRES 6.8)

# Backend C++ (biblioteca de núcleo), depois o app (executável + módulo QML).
add_subdirectory(src)
add_subdirectory(qml)

option(DISTRIBUIDORA_BUILD_TESTS "Compilar os testes automatizados" ON)
if(DISTRIBUIDORA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
