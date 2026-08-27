pragma Singleton
import QtQuick

// Tokens de design — Empório dos Amigos.
// Identidade: preto (primária) + laranja (secundária), neutros quentes, premium.
// Alterna claro/escuro via `Theme.dark` (espelha os dois temas do mockup).
QtObject {
    // Estado do tema. Alternado pelo botão da topbar. Padrão: escuro.
    property bool dark: true

    // --- Cores (claro : escuro) ---
    readonly property color ink:            dark ? "#000000" : "#14120C"  // ação primária preta (tom da logo no escuro)
    readonly property color inkStrong:      "#000000"
    readonly property color primary:        dark ? "#F0742E" : "#E2611E"  // laranja (acento)
    readonly property color primaryHover:   dark ? "#E2611E" : "#C24E12"
    readonly property color accentSoft:     dark ? "#2A1E12" : "#F7E9DD"
    readonly property color background:     dark ? "#000000" : "#F5F3F0"  // preto puro (tom da logo) no escuro
    readonly property color surface:        dark ? "#0C0B09" : "#FFFFFF"  // card levemente elevado sobre o preto
    readonly property color surfaceAlt:     dark ? "#141209" : "#FAF8F5"
    readonly property color sidebar:        "#000000"                     // preto da logo (escura sempre)
    readonly property color sidebarActive:  dark ? "#26221A" : "#26221A"
    readonly property color textOnDark:     "#F2EEE7"                     // sidebar é escura sempre
    readonly property color textOnDarkMuted: dark ? "#8B8474" : "#8E877A"
    readonly property color text:           dark ? "#EFEAE1" : "#1E1B15"
    readonly property color textMuted:      dark ? "#9A9285" : "#736C61"
    readonly property color border:         dark ? "#2A271F" : "#E7E2DB"
    readonly property color borderStrong:   dark ? "#3A362C" : "#D8D2C8"
    readonly property color success:        dark ? "#63C48C" : "#2E7D51"
    readonly property color danger:         dark ? "#E8877A" : "#BC3B2A"
    readonly property color warning:        dark ? "#D8AE5C" : "#A9741A"

    // --- Espaçamento ---
    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 16
    readonly property int spacingLg: 24
    readonly property int spacingXl: 32

    readonly property int radius: 12
    readonly property int radiusSm: 8

    // --- Tipografia ---
    readonly property string fontBase: "Archivo"
    readonly property string fontDisplay: "Fraunces"
    readonly property string fontFamily: fontBase   // compat. com componentes

    readonly property int fontXs: 11
    readonly property int fontSm: 13
    readonly property int fontMd: 15
    readonly property int fontLg: 18
    readonly property int fontXl: 25
    readonly property int fontXxl: 31
}
