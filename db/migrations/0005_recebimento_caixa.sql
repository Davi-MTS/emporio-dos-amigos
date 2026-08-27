-- =============================================================================
-- Migration 0005 — Recebimento de fiado como movimento de caixa
-- =============================================================================
-- Quando um cliente paga a dívida (fiado) EM DINHEIRO, esse dinheiro entra na
-- gaveta e precisa aparecer no fechamento. Antes, `quitar/receber` só mudava o
-- status da conta e o dinheiro recebido ficava invisível para o caixa.
--
-- Adiciona o tipo 'recebimento' em mov_caixa. Como o CHECK de uma tabela não
-- pode ser alterado no SQLite, recriamos a tabela (nada referencia mov_caixa,
-- então é seguro com foreign_keys ligado).
-- =============================================================================

CREATE TABLE mov_caixa_nova (
    id         INTEGER PRIMARY KEY,
    sessao_id  INTEGER NOT NULL REFERENCES sessoes_caixa(id) ON DELETE CASCADE,
    tipo       TEXT NOT NULL CHECK (tipo IN ('sangria','suprimento','recebimento')),
    valor      INTEGER NOT NULL,                        -- centavos
    motivo     TEXT,
    usuario_id INTEGER REFERENCES usuarios(id) ON DELETE SET NULL,
    data       TEXT NOT NULL DEFAULT (datetime('now'))
);

INSERT INTO mov_caixa_nova (id, sessao_id, tipo, valor, motivo, usuario_id, data)
    SELECT id, sessao_id, tipo, valor, motivo, usuario_id, data FROM mov_caixa;

DROP TABLE mov_caixa;

ALTER TABLE mov_caixa_nova RENAME TO mov_caixa;

CREATE INDEX idx_mov_caixa_sessao ON mov_caixa(sessao_id);
