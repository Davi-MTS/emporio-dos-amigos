# Empório dos Amigos — Sistema de Gestão

Sistema de gestão (ERP + PDV) para distribuidora de bebidas — **loja única**.
Desktop nativo em **Qt 6 + QML**, regras de negócio em **C++**, banco **SQLite**
(**offline-first**, sem internet obrigatória), alvo **Windows**.

## O que o sistema faz

- **PDV / frente de caixa** — leitor de código de barras (bipe + Enter), busca por
  nome, quantidade digitável, escolha de unidade/caixa, múltiplas formas de
  pagamento com troco, desconto e venda no fiado.
- **Produtos compostos ("copão")** — receita por **categoria** (ex.: destilado em
  ml, gelo em unidade); na venda você escolhe o produto exato de cada categoria e
  o estoque baixa os **insumos**, não o copão.
- **Estoque** — entrada de mercadoria, inventário e retirada (perda/quebra), com
  **custo médio ponderado** e conversão de embalagem (unidade ↔ caixa/fardo).
- **Caixa** — abertura, sangria, suprimento e **fechamento conferido** (esperado
  × contado × diferença).
- **Compras** — fornecedores, entrada por nota fiscal (nº e data), geração de
  conta a pagar.
- **Clientes e fiado** — limite de crédito, recebimento total ou **parcial**.
- **Financeiro** — contas a pagar/receber; pagamento em dinheiro vira sangria.
- **Relatórios** — faturamento, lucro (custo travado no momento da venda), ticket
  médio, formas de pagamento, mais vendidos e produtos parados.
- **Backup** automático ao fechar o caixa (5 cópias) e **restauração**.
- **Relatório para o celular** — HTML gerado numa pasta do OneDrive.

## Compilar e rodar

Pré-requisitos: **Qt 6.8+** (módulos Quick, QuickControls2, Sql, Network),
**CMake 3.21+** e **Ninja**. O ambiente de referência é o **MinGW** que acompanha
o Qt.

Caminhos do ambiente de referência:

| Ferramenta | Caminho |
| --- | --- |
| Qt | `C:\Qt\6.8.3\mingw_64` |
| Compilador | `C:\Qt\Tools\mingw1310_64\bin` |
| CMake / CTest | `C:\Qt\Tools\CMake_64\bin` |
| Ninja | `C:\Qt\Tools\Ninja` |

Configurar (uma vez):

```bash
cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/mingw_64
```

Compilar:

```bash
cmake --build build/mingw
```

Rodar os testes:

```bash
ctest --test-dir build/mingw --output-on-failure
```

O executável fica em `build/mingw/distribuidora.exe`. Para rodar, o
`C:\Qt\6.8.3\mingw_64\bin` e o `C:\Qt\Tools\mingw1310_64\bin` precisam estar no
`PATH` (ou use o "Qt 6.8.3 (MinGW) Command Prompt").

> Os presets em `CMakePresets.json` também funcionam (`cmake --preset
> windows-debug`), desde que `CMAKE_PREFIX_PATH` aponte para o Qt.

## Dados

O banco é criado sozinho na primeira execução em:

```
%APPDATA%\Distribuidora\Distribuidora\distribuidora.db
```

O schema evolui por **migrations versionadas** (`db/migrations/`), aplicadas
automaticamente ao abrir o app. **O banco não vai para o Git** — ele contém os
dados reais da loja.

No primeiro acesso o sistema pede a criação do **usuário administrador**.

## Convenções importantes

- **Dinheiro em centavos** (inteiro) — nunca `float`.
- **Custo por unidade em milésimos de centavo** — preserva frações (custo por ml).
- **Quantidades sempre na unidade base** do produto; embalagem é conversão.
- **Datas em hora local** (o UTC do SQLite jogava vendas noturnas para o dia
  seguinte).
- **Nunca guardar array de objetos em `ListModel`** do QML — serializar para JSON.

## Documentação

- `CLAUDE.md` — briefing completo: decisões, arquitetura e histórico de cada fase.
- `docs/modelo-de-dados.md` — esquema das tabelas.
- `docs/design-ui.md` — identidade visual e padrões de interface.
- `docs/plano-backup.md` / `docs/plano-mobile.md` — planos das funcionalidades.

## Estrutura

- `src/` — C++ (`domain/` regras de negócio, `services/` transversais,
  `models/` models de lista para o QML, `app/` fachada + `main`).
- `qml/` — telas (`screens/`) e componentes (`components/`, `theme/`).
- `db/migrations/` — evolução do schema (SQL versionado, aplicado em ordem).
- `tests/` — testes automatizados (QtTest, registrados no CTest).
- `resources/` — fontes e a logo da marca.
- `deploy/` — empacotamento para Windows.
