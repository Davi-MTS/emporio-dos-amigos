#pragma once

#include <QJsonObject>
#include <QSqlDatabase>
#include <QString>

// Gera um relatório-resumo em HTML (arquivo único, autossuficiente) para o dono
// abrir no celular. O arquivo fica numa pasta LOCAL do app e é entregue anexado
// na mensagem do Telegram — sem depender de nuvem/OneDrive. Só leitura;
// atualizado ao fechar o caixa e no botão de backup.
//
// Reaproveita os cálculos de relatório/estoque/financeiro — é uma camada de
// EXPORTAÇÃO, sem regra de negócio nova. Fica no núcleo (sem QML) para ser testável.
class RelatorioMobileService
{
public:
    // destinoDir vazio => pasta padrão local do app (AppDataLocation/Relatorio).
    explicit RelatorioMobileService(QSqlDatabase db, const QString &destinoDir = QString());

    QString diretorio() const { return m_dir; }
    QString caminhoArquivo() const;

    // Monta o HTML e grava no arquivo. Preenche *out com o caminho. false em erro.
    bool gerar(QString *out = nullptr);

    // Resumo curto e formatado para enviar como mensagem (Telegram).
    QString resumoTexto() const;

    QString ultimoErro() const { return m_erro; }

    static QString pastaPadrao();

private:
    // Coleta única dos dados do negócio; alimenta tanto o HTML quanto o resumo.
    QJsonObject coletarDados() const;
    QString montarHtml() const;

    QSqlDatabase m_db;
    QString m_dir;
    QString m_erro;
};
