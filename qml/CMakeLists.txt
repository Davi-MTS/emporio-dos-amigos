# =============================================================================
# qml/ — aplicativo (executável + módulo QML da interface)
# =============================================================================
# O executável é definido aqui (e não em src/) para que os arquivos .qml sejam
# locais a este diretório: o qmlcachegen não lida bem com caminhos contendo "..".
# O main.cpp é referenciado por caminho absoluto (é só C++, sem esse problema).
# =============================================================================

# A interface vive numa BIBLIOTECA (distribuidora_ui), não no executável, para
# que os testes de QML (tests/qml) possam carregar as mesmas telas que a loja
# usa. Se ficasse no executável, os .qml iriam para dentro do .exe e nenhum
# outro binário conseguiria abri-los.
qt_add_library(distribuidora_ui STATIC
    ${CMAKE_SOURCE_DIR}/src/app/ProdutoFotoProvider.h
    ${CMAKE_SOURCE_DIR}/src/app/ProdutoFotoProvider.cpp
)

# Theme.qml é um singleton (pragma Singleton no arquivo). A propriedade precisa
# ser definida ANTES de qt_add_qml_module para entrar no qmldir gerado.
set_source_files_properties(theme/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(distribuidora_ui
    URI Distribuidora
    VERSION 1.0
    QML_FILES
        Main.qml
        theme/Theme.qml
        components/Sidebar.qml
        components/NavButton.qml
        components/AppIcon.qml
        components/AppButton.qml
        components/AppTextField.qml
        components/AppDateField.qml
        components/AppComboBox.qml
        components/AppSpinBox.qml
        components/AppDialog.qml
        components/SegmentedControl.qml
        components/ToggleButton.qml
        components/PlaceholderScreen.qml
        components/FotoProduto.qml
        components/StatusBadge.qml
        components/FormField.qml
        screens/dashboard/DashboardScreen.qml
        screens/produtos/ProdutosScreen.qml
        screens/estoque/EstoqueScreen.qml
        screens/vencimento/VencimentoScreen.qml
        screens/pdv/PdvScreen.qml
        screens/caixa/CaixaScreen.qml
        screens/usuarios/LoginScreen.qml
        screens/usuarios/UsuariosScreen.qml
        screens/compras/ComprasScreen.qml
        screens/clientes/ClientesScreen.qml
        screens/financeiro/FinanceiroScreen.qml
        screens/relatorios/RelatoriosScreen.qml
        screens/vendas/VendasScreen.qml
        screens/config/BackupScreen.qml
)

# Fontes embutidas (:/fonts/...), carregadas em main.cpp.
qt_add_resources(distribuidora_ui "fontes"
    PREFIX "/fonts"
    BASE ${CMAKE_SOURCE_DIR}/resources/fonts
    FILES
        ${CMAKE_SOURCE_DIR}/resources/fonts/Archivo-Variable.ttf
        ${CMAKE_SOURCE_DIR}/resources/fonts/Fraunces-Variable.ttf
)

# Imagens embutidas (:/images/...): logo da marca etc.
qt_add_resources(distribuidora_ui "imagens"
    PREFIX "/images"
    BASE ${CMAKE_SOURCE_DIR}/resources/images
    FILES
        ${CMAKE_SOURCE_DIR}/resources/images/logo.png
)

target_link_libraries(distribuidora_ui
    PUBLIC
        Distribuidora::Core
        Qt6::Quick
        Qt6::QuickControls2
)

# --- Executável ---------------------------------------------------------------
qt_add_executable(distribuidora
    ${CMAKE_SOURCE_DIR}/src/app/main.cpp
)

# Sem janela de console no Windows quando for build de produção (Release).
# Como CUI, o app abria um cmd preto junto — no PC da loja isso confunde e, se o
# operador fechar aquela janela, o sistema morre no meio de uma venda.
# Em Debug o console CONTINUA, porque é onde saem os avisos de QML e migrations.
set_target_properties(distribuidora PROPERTIES
    WIN32_EXECUTABLE $<CONFIG:Release>
)

# Liga o PLUGIN do módulo (e não só a biblioteca): é ele que inicializa os
# recursos :/qt/qml/Distribuidora/... para o "import Distribuidora" funcionar.
target_link_libraries(distribuidora
    PRIVATE
        distribuidora_ui
        distribuidora_uiplugin
)
