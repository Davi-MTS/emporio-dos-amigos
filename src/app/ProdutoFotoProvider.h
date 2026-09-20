#pragma once

#include <QQuickImageProvider>
#include <QSqlDatabase>
#include <QString>

// Entrega ao QML as fotos que estão guardadas no banco.
//
// Uso no QML:  Image { source: "image://produto/" + id + "?v=" + App.versaoFotos }
//
// O "?v=" existe porque o Qt guarda a imagem em cache pela URL: sem ele, trocar
// a foto de um produto continuaria mostrando a antiga até fechar o programa.
// A parte depois do "?" é ignorada aqui, só serve para mudar a URL.
//
// THREADS — o motivo de o sistema ter caído na loja ao pôr foto em produto:
// com `asynchronous: true` o Qt chama requestImage() numa thread separada, e
// pode chamar de mais de uma. Uma conexão do Qt SQL só pode ser usada pela
// thread que a criou; usar a da thread principal daqui derrubava o programa
// assim que a lista recarregava ao mesmo tempo. Por isso o provider NÃO guarda
// conexão: guarda só o caminho do arquivo, e cada thread abre a sua, somente
// leitura (o banco está em WAL, então leitor e escritor convivem).
//
// Vive na camada de interface (não no núcleo) porque depende de QtQuick.
class ProdutoFotoProvider : public QQuickImageProvider
{
public:
    // Recebe a conexão principal só para ler dela o caminho do arquivo — na
    // thread que cria o provider, onde isso é permitido.
    explicit ProdutoFotoProvider(const QSqlDatabase &db);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    static const char *nome() { return "produto"; }

private:
    QByteArray lerFoto(int produtoId);

    const QString m_caminhoBanco;
};
