#pragma once

#include <QQuickImageProvider>
#include <QSqlDatabase>

// Entrega ao QML as fotos que estão guardadas no banco.
//
// Uso no QML:  Image { source: "image://produto/" + id + "?v=" + App.versaoFotos }
//
// O "?v=" existe porque o Qt guarda a imagem em cache pela URL: sem ele, trocar
// a foto de um produto continuaria mostrando a antiga até fechar o programa.
// A parte depois do "?" é ignorada aqui, só serve para mudar a URL.
//
// Vive na camada de interface (não no núcleo) porque depende de QtQuick.
class ProdutoFotoProvider : public QQuickImageProvider
{
public:
    explicit ProdutoFotoProvider(QSqlDatabase db);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    static const char *nome() { return "produto"; }

private:
    QSqlDatabase m_db;
};
