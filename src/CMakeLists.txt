# =============================================================================
# src/ — backend C++ (biblioteca de núcleo)
# =============================================================================
# DistribuidoraCore reúne toda a regra de negócio. NÃO depende de QML/Quick,
# para ser testável isolada (ver princípios em CLAUDE.md). Link: Qt6::Core + Sql.
#
# O executável (distribuidora) e o módulo QML são definidos em qml/CMakeLists.txt,
# onde os arquivos .qml são locais (evita caminhos com ".." no qmlcache).
# =============================================================================

qt_add_library(DistribuidoraCore STATIC)
add_library(Distribuidora::Core ALIAS DistribuidoraCore)

target_sources(DistribuidoraCore
    PRIVATE
        database/Database.h
        database/Database.cpp
        database/MigrationRunner.h
        database/MigrationRunner.cpp
        utils/Money.h
        utils/Money.cpp
        domain/produtos/EmbalagemConverter.h
        domain/produtos/EmbalagemConverter.cpp
        domain/produtos/Produto.h
        domain/produtos/ProdutoRepository.h
        domain/produtos/ProdutoRepository.cpp
        domain/estoque/EstoqueRepository.h
        domain/estoque/EstoqueRepository.cpp
        domain/caixa/CaixaRepository.h
        domain/caixa/CaixaRepository.cpp
        domain/vendas/VendaRepository.h
        domain/vendas/VendaRepository.cpp
        domain/usuarios/Usuario.h
        domain/usuarios/UsuarioRepository.h
        domain/usuarios/UsuarioRepository.cpp
        domain/compras/Fornecedor.h
        domain/compras/FornecedorRepository.h
        domain/compras/FornecedorRepository.cpp
        domain/compras/Compra.h
        domain/compras/CompraRepository.h
        domain/compras/CompraRepository.cpp
        domain/clientes/Cliente.h
        domain/clientes/ClienteRepository.h
        domain/clientes/ClienteRepository.cpp
        domain/financeiro/FinanceiroRepository.h
        domain/financeiro/FinanceiroRepository.cpp
        domain/relatorios/RelatorioRepository.h
        domain/relatorios/RelatorioRepository.cpp
        services/auth/AuthService.h
        services/auth/AuthService.cpp
        services/backup/BackupService.h
        services/backup/BackupService.cpp
        services/relatoriomobile/RelatorioMobileService.h
        services/relatoriomobile/RelatorioMobileService.cpp
        services/log/LogService.h
        services/log/LogService.cpp
        services/telegram/TelegramService.h
        services/telegram/TelegramService.cpp
        models/ProdutosListModel.h
        models/ProdutosListModel.cpp
        models/EstoqueListModel.h
        models/EstoqueListModel.cpp
        models/UsuariosListModel.h
        models/UsuariosListModel.cpp
        models/FornecedoresListModel.h
        models/FornecedoresListModel.cpp
        models/ComprasListModel.h
        models/ComprasListModel.cpp
        models/ClientesListModel.h
        models/ClientesListModel.cpp
        models/ContasPagarModel.h
        models/ContasPagarModel.cpp
        models/VendasListModel.h
        models/VendasListModel.cpp
        models/ContasReceberModel.h
        models/ContasReceberModel.cpp
        app/AppBackend.h
        app/AppBackend.cpp
)

# Headers incluídos como "database/Database.h" a partir da raiz de src/.
target_include_directories(DistribuidoraCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(DistribuidoraCore
    PUBLIC
        Qt6::Core
        Qt6::Sql
        Qt6::Network   # QPasswordDigestor (PBKDF2) vive no QtNetwork
        Qt6::Gui       # QImage: reduz a foto do produto antes de gravar
)

# Scripts SQL embutidos como recursos (:/db/...), para acompanharem o executável.
# IMPORTANTE: ao criar uma nova migration/seed, adicione o arquivo aqui.
qt_add_resources(DistribuidoraCore "db_scripts"
    PREFIX "/db"
    BASE ${CMAKE_SOURCE_DIR}/db
    FILES
        ${CMAKE_SOURCE_DIR}/db/migrations/0001_schema_inicial.sql
        ${CMAKE_SOURCE_DIR}/db/migrations/0002_venda_troco.sql
        ${CMAKE_SOURCE_DIR}/db/migrations/0003_produto_composto.sql
        ${CMAKE_SOURCE_DIR}/db/migrations/0004_composicao_por_categoria.sql
        ${CMAKE_SOURCE_DIR}/db/migrations/0005_recebimento_caixa.sql
        ${CMAKE_SOURCE_DIR}/db/migrations/0006_movimentacao_custo.sql
        ${CMAKE_SOURCE_DIR}/db/migrations/0007_custo_milesimos.sql
        ${CMAKE_SOURCE_DIR}/db/migrations/0008_compra_nota.sql
        ${CMAKE_SOURCE_DIR}/db/migrations/0009_datas_locais.sql
        ${CMAKE_SOURCE_DIR}/db/migrations/0010_cancelar_venda.sql
        ${CMAKE_SOURCE_DIR}/db/migrations/0011_perfil_funcionario.sql
        ${CMAKE_SOURCE_DIR}/db/migrations/0012_conta_forma_pagamento.sql
        ${CMAKE_SOURCE_DIR}/db/migrations/0013_produto_dose.sql
        ${CMAKE_SOURCE_DIR}/db/migrations/0014_produto_foto.sql
        ${CMAKE_SOURCE_DIR}/db/seed/0001_dados_iniciais.sql
)
