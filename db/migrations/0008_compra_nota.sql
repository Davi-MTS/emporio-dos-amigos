-- =============================================================================
-- Migration 0008 — Nº e data da nota fiscal na compra
-- =============================================================================
-- Permite lançar uma compra identificando a NOTA FISCAL de origem (número + data
-- da nota). A `compras.data` continua sendo a data do registro no sistema; a
-- `data_nota` é a data de emissão impressa na nota.
-- =============================================================================

ALTER TABLE compras ADD COLUMN numero_nota TEXT;  -- nº da NF (opcional)
ALTER TABLE compras ADD COLUMN data_nota   TEXT;  -- data de emissão (ISO, opcional)
