# Plano de implementação — Backup e Restauração

> ✅ **IMPLEMENTADO.** Este é o plano original, mantido como registro das
> decisões. O que foi entregue está em `CLAUDE.md` e o uso no dia a dia em
> [instalacao.md](instalacao.md).
> ✅ A **cópia externa** (item 12.4) foi resolvida depois, por outro caminho:
> ao fechar o caixa, o Telegram leva o relatório em HTML **e o `.db` do backup**
> anexado (opção "Enviar também a cópia de segurança", na tela de Backup, ligada
> por padrão). O histórico da loja deixou de existir só num HD. Limite do
> Telegram: 45 MB por arquivo — acima disso o app avisa em vez de falhar calado.
> Sistema: Empório dos Amigos (Qt6 + QML + C++ + SQLite, desktop Windows, offline, loja única).

---

## 1. Objetivo

O banco é **todo o negócio** (vendas, caixa, fiado, estoque, custo, clientes). Hoje ele
vive num único arquivo, numa única máquina, sem nenhuma cópia automática. Um HD que
falha, um ransomware, um `distribuidora.db` apagado por engano = **perda total**.

O recurso de backup deve permitir, de forma **simples para um comerciante não técnico**:
1. **Salvar** uma cópia segura e íntegra do banco (manual e automática).
2. **Guardar** várias cópias com rotação (não encher o disco).
3. **Restaurar** o sistema a partir de uma cópia, com segurança.
4. Idealmente, mandar a cópia para um **local externo** (pen drive / pasta do OneDrive).

## 2. Estado atual

- Banco em `%APPDATA%/Distribuidora/Distribuidora/distribuidora.db` (modo **WAL**;
  gera os sidecars `-wal` e `-shm`).
- Migrations versionadas (`schema_migrations`) já sobem/atualizam o schema no início.
- Não existe nenhuma rotina de backup no app. O único backup até agora foi manual,
  renomeando o arquivo por fora (feito na conversa).

## 3. Requisitos

### Funcionais
- **F1 — Backup manual:** botão "Fazer backup agora" gera um arquivo íntegro.
- **F2 — Backup automático:** disparado em momentos-chave (ver §6), sem o usuário pensar.
- **F3 — Lista de backups:** mostrar cópias existentes com data/hora, tamanho e um
  resumo legível (ex.: "24/08 23:10 · 128 vendas · 340 produtos").
- **F4 — Restaurar:** escolher uma cópia e voltar o sistema para ela, com confirmação.
- **F5 — Retenção/rotação:** manter sempre as **5** cópias mais recentes. Ao criar a 6ª,
  apagar a mais antiga (mantém 5 fixas). **[definido]**
- **F6 — Local do backup:** pasta padrão local + opção de **também** copiar para um
  local externo escolhido (pen drive/OneDrive).
- **F7 — Exportar/importar arquivo avulso:** salvar/abrir um `.db` num caminho à escolha
  (para levar em pen drive ou mandar por outro meio).

### Não-funcionais
- **NF1 — Integridade:** a cópia deve ser um snapshot **consistente** (nada de copiar o
  arquivo "no meio" de uma escrita, com o WAL pela metade).
- **NF2 — Não bloquear a venda:** backup deve rodar com o banco aberto, rápido, sem travar.
- **NF3 — Simplicidade:** pouca decisão para o usuário; defaults sensatos.
- **NF4 — Segurança na restauração:** nunca sobrescrever o banco atual sem antes
  guardar uma cópia de emergência dele.
- **NF5 — Compatível com migrations:** restaurar um backup mais antigo deve funcionar —
  ao reabrir, as migrations atualizam o schema automaticamente.

## 4. Abordagem técnica

### Como copiar (o ponto crítico com WAL)
Copiar o `.db` "na mão" é perigoso em modo WAL (dados podem estar só no `-wal`).
Estratégia escolhida: **`VACUUM INTO 'caminho'`**.

- Disponível no SQLite ≥ 3.27; o Qt 6.8.3 embute versão muito mais nova (confirmado).
- Gera **um único arquivo** já compactado e **consistente**, lendo inclusive o que está
  no WAL, **sem fechar** a conexão nem travar a venda.
- Alternativa considerada e descartada: `PRAGMA wal_checkpoint(TRUNCATE)` + cópia de
  arquivo (mais frágil e com mais partes móveis). `VACUUM INTO` é mais simples e robusto.

```sql
VACUUM INTO 'C:/.../Backups/distribuidora-20260824-231005.db';
```

### Como restaurar (sem fechar o app no meio da sessão)
Trocar o arquivo com o banco aberto é arriscado. Abordagem segura em 2 tempos:

1. Ao confirmar a restauração, o app **não** troca na hora: ele
   - faz um **backup de emergência** do banco atual (VACUUM INTO `pre-restauracao-...`),
   - grava um marcador de "restauração pendente" (arquivo `.restore` apontando para o backup escolhido),
   - pede para **fechar e reabrir** o app.
2. No **próximo início**, ANTES de abrir a conexão, o app vê o marcador e:
   - substitui `distribuidora.db` pela cópia escolhida (removendo `-wal`/`-shm`),
   - apaga o marcador,
   - segue o boot normal (migrations atualizam o schema se o backup for mais antigo).

Isso evita qualquer estado inconsistente e não exige fechar conexões no meio do uso.

### Metadados (para a lista ficar amigável)
Ao gerar cada backup, salvar um sidecar `.json` com:
`{ criado_em, versao_app, versao_schema, tamanho, contagens: {vendas, produtos, clientes, ...} }`.
A tela de backups lê esses `.json` para mostrar o resumo legível (F3).

### Local dos arquivos
- Pasta padrão: `Documentos/Empório dos Amigos/Backups/` (via `QStandardPaths::DocumentsLocation`),
  fora do `%APPDATA%` para não sumir junto se o perfil for limpo.
- Nome: `distribuidora-AAAAMMDD-HHMMSS.db` (+ `.json` ao lado).
- Local externo (F6): caminho escolhido pelo usuário e lembrado nas configurações.

## 5. Arquitetura no código

Novos arquivos (seguindo a separação atual Core ↔ QML):

| Arquivo | Papel |
|---|---|
| `src/services/backup/BackupService.h/.cpp` | Toda a lógica: `criarBackup(destinoDir)`, `listarBackups()`, `agendarRestauracao(path)`, `aplicarRestauracaoPendente()` (chamado no boot), `rotacionar(max)`, cálculo de metadados. Usa `VACUUM INTO`. Sem dependência de QML. |
| `src/services/backup/BackupInfo.h` | Struct com metadados de um backup (para lista). |
| `src/app/main.cpp` | Chamar `BackupService::aplicarRestauracaoPendente()` **antes** de abrir o banco. |
| `src/app/AppBackend.{h,cpp}` | Fachada QML: `Q_INVOKABLE` para backup/restore/list/config (ver §7). |
| `qml/screens/config/BackupScreen.qml` (ou aba em Config) | UI (ver §8). |
| `tests/cpp/tst_backup_service.cpp` | Testes (ver §9). |
| Configurações | `QSettings` (org/app já definidos) para: pasta externa, auto-backup on/off, frequência, retenção. Alternativa: tabela `configuracoes` no banco — mas backup precisa funcionar mesmo com o banco problemático, então **`QSettings` é melhor** aqui. |

## 6. Gatilhos de backup automático (F2)

**[definido] Gatilho único: ao fechar o caixa** (`fecharCaixa`) — momento natural de
"fim de expediente", garante 1 backup por dia com os dados completos do turno.

Após criar, aplica a **retenção de 5** (F5): apaga a mais antiga se passar de 5.
(Sair do app e periódico foram descartados nesta decisão.)

## 7. API exposta ao QML (`AppBackend`)

```
Q_INVOKABLE QVariantMap fazerBackup();                 // { ok, caminho, resumo, erro }
Q_INVOKABLE QVariantList backupsDisponiveis();         // [{ caminho, criadoEm, tamanho, resumo }]
Q_INVOKABLE QVariantMap exportarBackupPara(path);      // salva cópia num caminho escolhido
Q_INVOKABLE bool agendarRestauracao(caminho);          // agenda p/ o próximo início
Q_INVOKABLE QVariantMap configBackup();                // { auto, frequencia, retencao, pastaExterna }
Q_INVOKABLE bool salvarConfigBackup(dados);
```

## 8. UI proposta

Uma aba **"Backup"** (dentro de uma futura tela de **Configurações**, ou como item na
retaguarda visível só para o Administrador):

- Cartão de status: "Último backup: hoje 23:10 ✓" (verde) / "há 3 dias ⚠" (alerta).
- Botão grande **"Fazer backup agora"** → roda `VACUUM INTO`, mostra confirmação com o resumo.
- Botão **"Salvar em pen drive…"** (abre seletor de pasta) → exporta cópia para lá.
- **Lista de backups** (data, tamanho, resumo) com ação **"Restaurar"** por linha →
  diálogo de confirmação forte ("isto vai substituir os dados atuais; uma cópia de
  segurança do estado atual será criada; o app vai reiniciar").
- Seção **Automático**: liga/desliga, frequência, quantas cópias manter, pasta externa.

Acesso deve ser **restrito ao Administrador** (permissão), por ser sensível.

## 9. Testes (`tst_backup_service`)

- **Cópia íntegra:** cria banco temp com dados → `criarBackup` → abre o backup e confere
  que as contagens/linhas batem com o original.
- **Round-trip de restauração:** altera o original → `agendarRestauracao(backup)` →
  simula o passo de boot (`aplicarRestauracaoPendente`) → confere que os dados voltaram
  ao estado do backup e que `-wal/-shm` foram removidos.
- **Backup de emergência:** ao restaurar, confirma que a cópia pré-restauração foi criada.
- **Retenção:** cria N+extra backups → `rotacionar(N)` → sobram só os N mais recentes.
- **Backup mais antigo + migrations:** restaura um `.db` com schema anterior e confere
  que as migrations sobem sem erro (compatibilidade).

## 10. Riscos e mitigação

| Risco | Mitigação |
|---|---|
| Cópia inconsistente (WAL) | `VACUUM INTO` (snapshot consistente). |
| Restauração corromper o banco vivo | Restauração **agendada no boot** + backup de emergência antes. |
| Disco cheio de backups | Retenção/rotação (F5). |
| Backup mais antigo (schema velho) | Migrations rodam no reabrir (NF5) — coberto por teste. |
| Pen drive removido / caminho externo some | Backup local sempre acontece; o externo é "melhor esforço" com aviso se falhar. |
| Usuário restaura por engano | Confirmação forte + restrição ao Administrador + cópia de emergência recuperável. |

## 11. Não-metas (fora deste escopo, por ora)

- Sincronização em nuvem / multi-dispositivo.
- Criptografia do backup (pode virar opção futura).
- Backup incremental/diferencial (o banco é pequeno; cópia full via `VACUUM INTO` basta).

## 12. Fases de implementação (incremental)

| Fase | Entrega | Esforço aprox. |
|---|---|---|
| **1 — Núcleo + manual** | `BackupService` (`VACUUM INTO`) + metadados + `fazerBackup`/`backupsDisponiveis` + testes de cópia. Botão "Fazer backup agora" e lista. | ~0,5–1 dia |
| **2 — Restauração** | Backup de emergência + marcador `.restore` + `aplicarRestauracaoPendente` no boot + diálogo de restaurar + teste round-trip. | ~0,5 dia |
| **3 — Automático + retenção + config** | Gatilhos (fechar caixa/sair) + rotação + tela de configurações + status "último backup". | ~0,5–1 dia |
| **4 — Externo (adiado)** | "Salvar em pen drive…" + pasta externa lembrada + aviso de falha. **Fora do escopo atual** — só backup local por ora. | ~0,5 dia |

## 13. Decisões (fechadas)

1. **Pasta padrão:** `Documentos/Empório dos Amigos/Backups/`. ✅
2. **Automático:** somente **ao fechar o caixa**. ✅
3. **Retenção:** manter sempre **5** cópias (na 6ª, apaga a mais antiga). ✅
4. **Cópia externa (pen drive/OneDrive):** **não** por enquanto — só backup local.
   Fica como possível evolução futura (Fase 4), sem impacto no restante. ✅
5. **Interface:** tela "Configurações → Backup", só Administrador. ✅

> **Sobre o item 4 (cópia externa):** o backup automático salva numa pasta **da própria
> máquina**. Se o HD queimar ou o PC for roubado, o backup vai junto. A "cópia externa"
> é gravar **também** o backup em um lugar **fora** do computador — um **pen drive USB**
> ou uma **pasta sincronizada na nuvem (OneDrive)** — para sobreviver a um desastre no PC.
> Como este projeto já roda dentro do OneDrive, apontar os backups para uma pasta do
> OneDrive daria proteção fora da máquina **de graça e automática**. Recomendo incluir
> isso (basta salvar a cópia também numa pasta configurável); se o pen drive não estiver
> conectado, o backup local continua acontecendo normalmente.
