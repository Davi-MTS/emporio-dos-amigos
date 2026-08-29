import QtQuick
import QtTest
import Distribuidora

// Tela de Backup: é ela que decide se a cópia do banco sai deste computador.
// Se o botão de "enviar também a cópia de segurança" deixar de existir ou de
// gravar, o backup volta a morrer no HD da loja sem ninguém perceber.
TestCase {
    id: caso
    name: "Backup"
    width: 1100
    height: 700
    visible: true
    when: windowShown

    Item { id: palco; anchors.fill: parent }
    Component { id: cBackup; BackupScreen {} }

    // A configuração guarda (e devolve) a opção de enviar o backup.
    function test_config_guarda_envio_do_backup() {
        App.salvarConfigTelegram("123:abc", "-100999", true, false);
        var c1 = App.configTelegram();
        compare(c1.enviaBackup, false, "desligado tem que continuar desligado");

        App.salvarConfigTelegram("123:abc", "-100999", true, true);
        var c2 = App.configTelegram();
        compare(c2.enviaBackup, true);
        compare(c2.chatId, "-100999");
        compare(c2.ativo, true);
    }

    // A tela precisa expor o botão e refletir o que está salvo.
    function test_tela_reflete_config() {
        App.salvarConfigTelegram("123:abc", "-100999", true, true);
        var t = createTemporaryObject(cBackup, palco, { width: 1060, height: 660 });
        verify(t !== null, cBackup.errorString());
        wait(0);

        var botao = achar(t, "Enviar também a cópia de segurança");
        verify(botao !== null, "o botão de enviar o backup sumiu da tela");
        compare(botao.checked, true, "deveria vir ligado, como está salvo");
    }

    // Sem o botão ligado, a tela tem que avisar que o backup fica só no PC.
    function test_tela_avisa_quando_desligado() {
        App.salvarConfigTelegram("123:abc", "-100999", true, false);
        var t = createTemporaryObject(cBackup, palco, { width: 1060, height: 660 });
        verify(t !== null, cBackup.errorString());
        wait(0);

        var botao = achar(t, "Enviar também a cópia de segurança");
        verify(botao !== null);
        compare(botao.checked, false);
        verify(temTextoContendo(t, "SÓ neste computador"),
               "sem o envio, a tela precisa avisar o risco");
    }

    // Procura um botão pelo texto (a tela não expõe ids para fora).
    function achar(raiz, texto) {
        var fila = [raiz];
        while (fila.length > 0) {
            var it = fila.shift();
            if (it.text !== undefined && it.text === texto && it.checkable === true)
                return it;
            for (var i = 0; i < it.children.length; i++)
                fila.push(it.children[i]);
        }
        return null;
    }

    function temTextoContendo(raiz, trecho) {
        var fila = [raiz];
        while (fila.length > 0) {
            var it = fila.shift();
            if (it.visible !== false && typeof it.text === "string" && it.text.indexOf(trecho) >= 0)
                return true;
            for (var i = 0; i < it.children.length; i++)
                fila.push(it.children[i]);
        }
        return false;
    }
}
