-- =============================================================================
-- 0017 — Custo pendente de venda feita sem estoque
-- =============================================================================
-- O PDV deixa vender sem estoque (decisão do dono: a venda não trava no balcão).
-- Só que o custo dessas unidades não existe no momento da venda: a venda gravava
-- o custo médio daquele instante e ele ficava travado para sempre. Produto que
-- nunca tinha entrado saía com custo ZERO — lucro de 100% — mesmo depois da
-- compra lançada.
--
-- `qtd_pendente_custo` guarda, em cada saída de venda, quantas unidades foram
-- vendidas além do que havia no estoque. Quando a mercadoria chega com custo, a
-- entrada acerta essas unidades com o custo da compra (as mais antigas primeiro)
-- e o lucro do dia da venda se corrige sozinho.
--
-- Vendas anteriores a esta migration ficam com 0: não dá para saber, olhando só
-- o saldo de hoje, quais delas saíram sem estoque.
-- =============================================================================

ALTER TABLE movimentacoes_estoque ADD COLUMN qtd_pendente_custo INTEGER NOT NULL DEFAULT 0;

CREATE INDEX idx_mov_pendente_custo
    ON movimentacoes_estoque(produto_id, id)
    WHERE qtd_pendente_custo > 0;
