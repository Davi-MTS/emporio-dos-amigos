# =============================================================================
# capturar_telas — ferramenta de DOCUMENTAÇÃO, não faz parte do programa da loja
# =============================================================================
# Gera as imagens do manual abrindo as telas de verdade. Fica fora do CTest e
# fora do pacote; existe para o manual poder ser refeito quando as telas mudarem,
# em vez de envelhecer em silêncio.
# =============================================================================

qt_add_executable(capturar_telas main.cpp)

target_link_libraries(capturar_telas
    PRIVATE
        Distribuidora::Core
        distribuidora_ui
        distribuidora_uiplugin
)
