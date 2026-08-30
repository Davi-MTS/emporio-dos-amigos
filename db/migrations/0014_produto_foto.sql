-- =============================================================================
-- 0014 — Foto do produto guardada no banco
-- =============================================================================
-- A tabela já nascia com uma coluna `foto TEXT` pensada para guardar o CAMINHO
-- de um arquivo. Ela nunca foi usada (sempre NULL) e o caminho é a solução
-- errada aqui: o backup é um arquivo só (VACUUM INTO) e é ele que sai da loja
-- pelo Telegram. Com a imagem numa pasta, restaurar traria os dados e perderia
-- as fotos — justo no dia em que o HD queimou.
--
-- Então a coluna velha sai e entra uma BLOB, com a imagem dentro. Para o banco
-- não inchar, ela é reduzida antes de gravar (lado maior 320 px, JPEG): 20 a
-- 30 KB por produto. Mil produtos com foto somam ~25 MB, dentro do limite de
-- 45 MB por arquivo do Telegram.
--
-- Nada se perde ao remover a coluna: ela está NULL em todos os registros e
-- nenhuma parte do sistema jamais leu ou escreveu nela.
-- =============================================================================

ALTER TABLE produtos DROP COLUMN foto;
ALTER TABLE produtos ADD COLUMN foto BLOB;
