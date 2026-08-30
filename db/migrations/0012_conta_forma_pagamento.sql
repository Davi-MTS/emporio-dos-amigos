-- =============================================================================
-- 0012 — Guardar COMO a conta foi paga
-- =============================================================================
-- Pagar uma conta em dinheiro tira o valor da gaveta (vira sangria no caixa).
-- Pagar por pix/cartão não encosta na gaveta. Só que a conta não guardava essa
-- diferença: depois de paga, ninguém sabia por onde o dinheiro tinha saído.
--
-- Sem isso não dá para DESFAZER um pagamento lançado por engano: se o valor
-- saiu da gaveta, o estorno precisa devolver o dinheiro para a gaveta; se saiu
-- do banco, não pode mexer no caixa. Guardar a forma é o que torna o estorno
-- possível sem chutar.
--
-- Contas já pagas ficam com forma_pagamento NULL — o estorno delas trata como
-- "não sei de onde saiu" e não mexe no caixa, só reabre a conta.
-- =============================================================================

ALTER TABLE contas_pagar ADD COLUMN forma_pagamento TEXT;
