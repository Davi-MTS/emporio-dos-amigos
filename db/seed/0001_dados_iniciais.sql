-- =============================================================================
-- Seed 0001 — Dados iniciais
-- =============================================================================
-- Dados de base carregados na primeira inicialização do banco (não é migration).
-- Usa INSERT OR IGNORE para ser idempotente (rodar de novo não duplica).
--
-- O usuário Administrador NÃO é criado aqui: a senha precisa ser gerada com
-- hash pela aplicação (ver AuthService). O app cria o admin no primeiro boot
-- se não houver nenhum usuário.
-- =============================================================================

-- Perfis. As permissões são um objeto JSON (perfis.permissoes).
INSERT OR IGNORE INTO perfis (id, nome, permissoes) VALUES
    (1, 'Administrador', json('{
        "tudo": true
    }')),
    (2, 'Funcionário', json('{
        "vende": true,
        "pode_dar_desconto": false,
        "pode_cancelar_venda": false,
        "ve_financeiro": false,
        "edita_produto": false,
        "edita_preco": false
    }'));

-- Categorias de produto mais comuns numa distribuidora de bebidas.
INSERT OR IGNORE INTO categorias (nome) VALUES
    ('Cerveja'),
    ('Refrigerante'),
    ('Energético'),
    ('Água'),
    ('Suco'),
    ('Destilados'),
    ('Vinho'),
    ('Gelo'),
    ('Carvão'),
    ('Snacks'),
    ('Tabacaria'),
    ('Descartáveis');
