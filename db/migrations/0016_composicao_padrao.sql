-- =============================================================================
-- 0016 — Composição padrão do copão e preço que se ajusta na troca
-- =============================================================================
-- Antes, o composto só dizia "uma bebida da categoria Destilados, um da
-- categoria Energético". Qual produto exatamente ficava para o operador
-- escolher na hora, e o PREÇO era digitado à mão em cada venda.
--
-- O modelo que a loja pediu: cada copão tem uma composição PADRÃO (o copão de
-- Beafeeter vem com Extra Power, gelo e copo) e um preço para essa combinação.
-- Trocar um item ajusta o preço pela DIFERENÇA entre o escolhido e o padrão —
-- Monster no lugar do Extra Power sobe o copão em R$ 5,00, que é o quanto um
-- custa a mais que o outro na prateleira.
--
--   produto_padrao_id -> o item que vem por padrão nesta linha
--   travada           -> linha que não se troca na venda. É o caso do próprio
--                        destilado: existe um copão para cada um, então trocar
--                        a bebida do "copão de Beafeeter" seria vender outra
--                        coisa com o nome errado.
-- =============================================================================

ALTER TABLE produto_composicao ADD COLUMN produto_padrao_id INTEGER REFERENCES produtos(id);
ALTER TABLE produto_composicao ADD COLUMN travada INTEGER NOT NULL DEFAULT 0;
