#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVariantMap>
#include <QVector>

// Metadados de um arquivo de backup (para listar na interface).
struct BackupInfo
{
    QString caminho;        // caminho absoluto do .db
    QString criadoEm;       // ISO 8601
    qint64 tamanho = 0;     // bytes
    QString resumo;         // ex.: "128 vendas · 340 produtos · 20 clientes"
    QVariantMap contagens;  // {vendas, produtos, clientes, compras}
};

// Backup e restauração do banco (SQLite). A cópia usa `VACUUM INTO`, que gera um
// arquivo único e CONSISTENTE mesmo com o banco aberto em modo WAL, sem travar a
// venda. A restauração é feita no PRÓXIMO início do app (troca segura do arquivo),
// nunca com a conexão aberta no meio da sessão.
//
// Fica no núcleo (sem QML) para ser testável. Cada backup ganha um sidecar .json
// com os metadados legíveis.
class BackupService
{
public:
    // backupsDir vazio => pasta padrão (Documentos/Empório dos Amigos/Backups).
    explicit BackupService(QSqlDatabase db, const QString &backupsDir = QString());

    QString diretorio() const { return m_dir; }

    // Cria um backup íntegro agora. Preenche *out (se != nullptr) com os metadados.
    bool criarBackup(BackupInfo *out = nullptr);

    // Backups existentes, do mais recente para o mais antigo.
    QVector<BackupInfo> listarBackups() const;

    // Mantém apenas os `manter` mais recentes; apaga o resto (.db + .json).
    bool rotacionar(int manter);

    // Agenda a restauração de `caminhoBackup` para o próximo início do app: faz um
    // backup de emergência do estado atual e grava um marcador ao lado do banco.
    bool agendarRestauracao(const QString &caminhoBackup);

    // Chamado no boot, ANTES de abrir o banco. Se houver marcador, substitui o
    // arquivo do banco pela cópia agendada e remove o marcador. Estático porque
    // roda sem nenhuma conexão aberta.
    static bool aplicarRestauracaoPendente(const QString &dbPath, QString *erro = nullptr);

    QString ultimoErro() const { return m_erro; }

    static QString pastaPadrao();

private:
    QVariantMap coletarContagens() const;
    static QString resumoDe(const QVariantMap &contagens);
    void escreverSidecar(const QString &dbPath, const BackupInfo &info) const;
    static BackupInfo lerSidecar(const QString &dbPath);
    static QString marcadorPath(const QString &dbPath);

    QSqlDatabase m_db;
    QString m_dir;
    QString m_erro;
};
