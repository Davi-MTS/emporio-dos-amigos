# =============================================================================
# tests/ — testes automatizados (QtTest)
# =============================================================================
# Foco nas partes que NÃO podem ter erro silencioso (CLAUDE.md):
# conversão de embalagem e valores monetários; mais integração das migrations.
# =============================================================================

find_package(Qt6 6.8 REQUIRED COMPONENTS Test)

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
