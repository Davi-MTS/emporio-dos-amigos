#pragma once

#include <QString>
#include <QStringList>

// Registro do que acontece no sistema, gravado em arquivo.
//
// Por que existe: o executável de produção não tem janela de console, então sem
// isto um erro (ou um fechamento inesperado) não deixaria rastro nenhum e não
// haveria como descobrir a causa depois, na loja.
//
// Captura tudo que passa por qDebug/qWarning/qCritical/qFatal — inclusive os
// erros de QML, que o Qt reporta por esse mesmo caminho.
namespace LogService {

// Instala o captador de mensagens. Chamar UMA vez, no começo do main().
void instalar();

// Arquivo atual: <dados do app>/logs/sistema.log
QString caminhoArquivo();
QString pasta();

// Últimas `n` linhas (mais recentes primeiro), para mostrar na interface.
QStringList ultimasLinhas(int n);

// Escreve uma linha de evento do negócio (ex.: "venda #12 cancelada").
void registrar(const QString &mensagem);

} // namespace LogService
