-- =============================================================================
-- 0011 — Perfil "Funcionário": conjunto de permissões estruturado
-- =============================================================================
-- O perfil tinha chaves que NINGUÉM lia ("pode_dar_desconto", "edita_preco"):
-- o dono lia "false" e achava que estava travado, mas o funcionário dava
-- desconto à vontade. E não havia chave alguma para relatório (lucro do dia) ou
-- ajuste de inventário — as duas coisas que mais interessam esconder.
--
-- Agora vale a regra: toda chave declarada aqui é lida em algum lugar do
-- sistema. Nada de permissão decorativa.
--
-- Administrador continua com {"tudo": true}, que atropela qualquer chave.
-- =============================================================================

UPDATE perfis SET permissoes = json('{
    "vende": true,
    "consulta_produtos": true,
    "recebe_mercadoria": true,
    "atende_cliente": true,
    "edita_produto": false,
    "pode_dar_desconto": false,
    "ajusta_estoque": false,
    "ve_relatorios": false,
    "ve_financeiro": false,
    "pode_cancelar_venda": false,
    "gerencia_usuarios": false
}') WHERE id = 2;
