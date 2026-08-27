# =============================================================================
# qml/ — aplicativo (executável + módulo QML da interface)
# =============================================================================
# O executável é definido aqui (e não em src/) para que os arquivos .qml sejam
# locais a este diretório: o qmlcachegen não lida bem com caminhos contendo "..".
# O main.cpp é referenciado por caminho absoluto (é só C++, sem esse problema).
# =============================================================================

qt_add_executable(distribuidora
    ${CMAKE_SOURCE_DIR}/src/app/main.cpp
)

# Theme.qml é um singleton (pragma Singleton no arquivo). A propriedade precisa
# ser definida ANTES de qt_add_qml_module para entrar no qmldir gerado.
set_source_files_properties(theme/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(distribuidora
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
        components/AppComboBox.qml
        components/AppSpinBox.qml
        components/AppDialog.qml
        components/SegmentedControl.qml
        components/ToggleButton.qml
        components/PlaceholderScreen.qml
        components/StatusBadge.qml
        components/FormField.qml
        screens/dashboard/DashboardScreen.qml
        screens/produtos/ProdutosScreen.qml
        screens/estoque/EstoqueScreen.qml
        screens/pdv/PdvScreen.qml
        screens/usuarios/LoginScreen.qml
        screens/usuarios/UsuariosScreen.qml
        screens/compras/ComprasScreen.qml
        screens/clientes/ClientesScreen.qml
        screens/financeiro/FinanceiroScreen.qml
        screens/relatorios/RelatoriosScreen.qml
        screens/config/BackupScreen.qml
        screens/config/RelatorioMobileScreen.qml
)

# Fontes embutidas (:/fonts/...), carregadas em main.cpp.
qt_add_resources(distribuidora "fontes"
    PREFIX "/fonts"
    BASE ${CMAKE_SOURCE_DIR}/resources/fonts
    FILES
        ${CMAKE_SOURCE_DIR}/resources/fonts/Archivo-Variable.ttf
        ${CMAKE_SOURCE_DIR}/resources/fonts/Fraunces-Variable.ttf
)

# Imagens embutidas (:/images/...): logo da marca etc.
qt_add_resources(distribuidora "imagens"
    PREFIX "/images"
    BASE ${CMAKE_SOURCE_DIR}/resources/images
    FILES
        ${CMAKE_SOURCE_DIR}/resources/images/logo.png
)

target_link_libraries(distribuidora
    PRIVATE
        Distribuidora::Core
        Qt6::Quick
        Qt6::QuickControls2
)
