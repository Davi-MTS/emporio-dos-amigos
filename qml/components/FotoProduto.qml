import QtQuick
import QtQuick.Controls
import Distribuidora

// Miniatura da foto do produto. Sem foto, mostra a inicial do nome num quadrado
// neutro — em vez de um buraco na lista, que faz a linha parecer quebrada.
Rectangle {
    id: raiz

    property int produtoId: 0
    property bool temFoto: false
    property string nome: ""
    property int lado: 36

    implicitWidth: lado
    implicitHeight: lado
    width: lado
    height: lado
    radius: 6
    color: Theme.surfaceAlt
    border.color: Theme.border
    border.width: raiz.temFoto ? 0 : 1
    clip: true

    Text {
        anchors.centerIn: parent
        visible: !raiz.temFoto
        text: raiz.nome.length > 0 ? raiz.nome.charAt(0).toUpperCase() : "?"
        color: Theme.textMuted
        font.pixelSize: Math.max(11, raiz.lado * 0.42)
        font.weight: Font.DemiBold
    }

    Image {
        anchors.fill: parent
        visible: raiz.temFoto
        // O "?v=" muda quando a foto é trocada: sem isso o Qt devolveria a
        // imagem antiga do cache.
        source: raiz.temFoto && raiz.produtoId > 0
                ? "image://produto/" + raiz.produtoId + "?v=" + App.versaoFotos
                : ""
        sourceSize.width: raiz.lado * 2
        sourceSize.height: raiz.lado * 2
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: false
    }
}
