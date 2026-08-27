-- =============================================================================
-- Migration 0007 — Custo por unidade em MILÉSIMOS de centavo
-- =============================================================================
-- Para o lucro ser preciso em unidades granulares (ex.: custo por ml), o custo
-- por unidade base passa a ser guardado em milésimos de centavo (centavos × 1000).
-- Antes, custos sub-centavo (comuns em ml) eram arredondados para 0, zerando o
-- custo médio e fazendo o lucro = faturamento.
--
-- Converte os valores existentes (que estavam em centavos) multiplicando por
-- 1000. A leitura para a interface e o relatório divide por 1000 nas bordas.
-- =============================================================================

UPDATE estoque
   SET custo_medio_unitario = custo_medio_unitario * 1000;

UPDATE movimentacoes_estoque
   SET custo_unit = custo_unit * 1000
 WHERE custo_unit IS NOT NULL;

UPDATE compra_itens
   SET custo_unit = custo_unit * 1000;
