#pragma once

#include <QSqlDatabase>
#include <QString>

// Gera um relatório-resumo em HTML (arquivo único, autossuficiente) para o dono
// abrir no celular. O arquivo é gravado numa pasta do OneDrive (sincroniza
// sozinho para a nuvem, acesso privado). Só leitura; atualizado ao fechar o
// caixa e por botão manual.
//
// Reaproveita os cálculos de relatório/estoque/financeiro — é uma camada de
// EXPORTAÇÃO, sem regra de negócio nova. Fica no núcleo (sem QML) para ser testável.
class RelatorioMobileService
{
public:
    // destinoDir vazio => pasta padrão (OneDrive/Empório dos Amigos/Relatório;
    // se não houver OneDrive, cai para Documentos).
    explicit RelatorioMobileService(QSqlDatabase db, const QString &destinoDir = QString());

    QString diretorio() const { return m_dir; }
    QString caminhoArquivo() const;

    // Monta o HTML e grava no arquivo. Preenche *out com o caminho. false em erro.
    bool gerar(QString *out = nullptr);

    QString ultimoErro() const { return m_erro; }

    static QString pastaPadrao();

private:
    QString montarHtml() const;

    QSqlDatabase m_db;
    QString m_dir;
    QString m_erro;
};
