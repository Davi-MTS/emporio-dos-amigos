# Distribuidora

Sistema de gestão (ERP) para distribuidora de bebidas — loja única.
Desktop nativo em **Qt 6 + QML**, backend **C++**, banco **SQLite** (offline-first),
alvo **Windows**.

## Como continuar o desenvolvimento com o Claude Code

1. Instale o Claude Code (ver https://docs.claude.com/en/docs/claude-code/overview).
2. Abra um terminal **na raiz desta pasta** (`distribuidora/`).
3. Rode `claude`.
4. O Claude Code lê automaticamente o `CLAUDE.md` da raiz, que contém todas as
   decisões do projeto (stack, conversão de embalagem, modelo de dados, fases).
5. Peça o próximo passo, por exemplo:
   *"Leia o CLAUDE.md e o docs/modelo-de-dados.md e gere o CMakeLists.txt e o
   esqueleto do projeto Qt 6."*

## Como compilar e rodar

Pré-requisitos: **Qt 6.8+** (com módulos Quick e Sql), **CMake 3.21+** e
**Ninja**. No Windows, use o toolchain MSVC do próprio Qt.

1. Aponte o CMake para a sua instalação do Qt (ajuste o caminho):

   ```bash
   set CMAKE_PREFIX_PATH=C:/Qt/6.8.0/msvc2022_64
   ```

2. Configure, compile e rode os testes com os presets:

   ```bash
   cmake --preset windows-debug
   ```

   ```bash
   cmake --build --preset windows-debug
   ```

   ```bash
   ctest --preset windows-debug
   ```

3. Rode o app: o executável fica em `build/windows-debug/src/distribuidora.exe`.
   O banco é criado automaticamente em `%APPDATA%/Distribuidora/distribuidora.db`
   na primeira execução.

## Documentação

- `CLAUDE.md` — briefing do projeto (decisões e princípios).
- `docs/modelo-de-dados.md` — esquema completo das tabelas.

## Estrutura

- `src/` — backend C++ (`domain/` = regras de negócio, `services/` = transversais).
- `qml/` — telas e componentes da interface.
- `db/migrations/` — evolução do schema (SQL versionado).
- `tests/` — testes de C++ e QML.
- `deploy/` — empacotamento para Windows.
