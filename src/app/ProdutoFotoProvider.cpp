#include "app/ProdutoFotoProvider.h"

#include <QImage>
#include <QSqlQuery>
#include <utility>

ProdutoFotoProvider::ProdutoFotoProvider(QSqlDatabase db)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_db(std::move(db))
{
}

QImage ProdutoFotoProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    // "12?v=3" -> 12. A parte do "?" só serve para furar o cache do Qt.
    const QString soId = id.section(QLatin1Char('?'), 0, 0);
    bool ok = false;
    const int produtoId = soId.toInt(&ok);
    if (!ok || produtoId <= 0)
        return {};

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT foto FROM produtos WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), produtoId);
    if (!q.exec() || !q.next())
        return {};

    const QByteArray dados = q.value(0).toByteArray();
    if (dados.isEmpty())
        return {};   // sem foto: a tela mostra o espaço vazio, não um erro

    QImage img;
    if (!img.loadFromData(dados, "JPEG"))
        return {};

    if (size)
        *size = img.size();

    // A foto já é pequena (320 px), mas se a tela pedir menor, entrega menor:
    // uma miniatura de 40 px numa lista longa não precisa carregar 320.
    if (requestedSize.isValid() && !requestedSize.isEmpty()
        && (requestedSize.width() < img.width() || requestedSize.height() < img.height())) {
        return img.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return img;
}
