-- =============================================================================
-- Migration 0010 — Cancelamento de venda
-- =============================================================================
-- Ao cancelar uma venda no fiado, a conta a receber correspondente não pode
-- simplesmente sumir (perderia a auditoria) nem continuar 'aberta' (cobraria um
-- cliente por algo cancelado). Passa a existir o status 'cancelada'.
--
-- O CHECK de uma tabela não pode ser alterado no SQLite; recriamos a tabela.
-- =============================================================================

CREATE TABLE contas_receber_nova (
    id         INTEGER PRIMARY KEY,
    cliente_id INTEGER NOT NULL REFERENCES clientes(id) ON DELETE RESTRICT,
    venda_id   INTEGER REFERENCES vendas(id) ON DELETE SET NULL,
    valor      INTEGER NOT NULL,
    vencimento TEXT,
    status     TEXT NOT NULL DEFAULT 'aberta'
                   CHECK (status IN ('aberta','paga','vencida','cancelada')),
    pago_em    TEXT
);

INSERT INTO contas_receber_nova (id, cliente_id, venda_id, valor, vencimento, status, pago_em)
    SELECT id, cliente_id, venda_id, valor, vencimento, status, pago_em FROM contas_receber;

DROP TABLE contas_receber;
ALTER TABLE contas_receber_nova RENAME TO contas_receber;

CREATE INDEX idx_contas_receber_cliente ON contas_receber(cliente_id);
CREATE INDEX idx_contas_receber_status  ON contas_receber(status);

-- Motivo e data do cancelamento, para saber por que a venda foi desfeita.
ALTER TABLE vendas ADD COLUMN cancelada_em TEXT;
ALTER TABLE vendas ADD COLUMN motivo_cancelamento TEXT;
