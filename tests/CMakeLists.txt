# =============================================================================
# tests/ — testes automatizados (QtTest)
# =============================================================================
# Foco nas partes que NÃO podem ter erro silencioso (CLAUDE.md):
# conversão de embalagem e valores monetários; mais integração das migrations.
# =============================================================================

find_package(Qt6 6.8 REQUIRED COMPONENTS Test QuickTest)

# Cada teste é um executável independente registrado no CTest.
function(distribuidora_add_test nome arquivo)
    qt_add_executable(${nome} ${arquivo})
    target_link_libraries(${nome} PRIVATE Distribuidora::Core Qt6::Test)
    add_test(NAME ${nome} COMMAND ${nome})
endfunction()

distribuidora_add_test(tst_money                cpp/tst_money.cpp)
distribuidora_add_test(tst_embalagem_converter  cpp/tst_embalagem_converter.cpp)
distribuidora_add_test(tst_migrations           cpp/tst_migrations.cpp)
distribuidora_add_test(tst_produto_repository    cpp/tst_produto_repository.cpp)
distribuidora_add_test(tst_estoque_repository    cpp/tst_estoque_repository.cpp)
distribuidora_add_test(tst_venda_repository      cpp/tst_venda_repository.cpp)
distribuidora_add_test(tst_caixa_repository      cpp/tst_caixa_repository.cpp)
distribuidora_add_test(tst_auth                  cpp/tst_auth.cpp)
distribuidora_add_test(tst_usuario_repository    cpp/tst_usuario_repository.cpp)
distribuidora_add_test(tst_compra_repository     cpp/tst_compra_repository.cpp)
distribuidora_add_test(tst_cliente_repository     cpp/tst_cliente_repository.cpp)
distribuidora_add_test(tst_financeiro_repository  cpp/tst_financeiro_repository.cpp)
distribuidora_add_test(tst_relatorio_repository   cpp/tst_relatorio_repository.cpp)
distribuidora_add_test(tst_composicao             cpp/tst_composicao.cpp)
distribuidora_add_test(tst_backup_service         cpp/tst_backup_service.cpp)
distribuidora_add_test(tst_relatorio_mobile       cpp/tst_relatorio_mobile.cpp)
distribuidora_add_test(tst_estorno                cpp/tst_estorno.cpp)
distribuidora_add_test(tst_dose                   cpp/tst_dose.cpp)

# -----------------------------------------------------------------------------
# Testes de INTERFACE (Qt Quick Test)
# -----------------------------------------------------------------------------
# Sobem as telas de verdade (módulo distribuidora_ui) contra um AppBackend num
# banco temporário. Cobrem o que os testes de C++ não alcançam: tela que não
# abre, diálogo maior que a janela, coluna fora da largura, carrinho perdendo
# os insumos do composto. Os casos ficam em casos/tst_*.qml.
qt_add_executable(tst_qml qml/arnes_qml.cpp)

target_link_libraries(tst_qml
    PRIVATE
        Distribuidora::Core
        distribuidora_ui
        distribuidora_uiplugin
        Qt6::QuickTest
)

target_compile_definitions(tst_qml PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml/casos"
)

# offscreen: roda em máquina sem tela (e no CI) sem abrir janela na cara de quem
# está usando o computador.
add_test(NAME tst_qml COMMAND tst_qml)
set_tests_properties(tst_qml PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FORCE_STDERR_LOGGING=1"
)
