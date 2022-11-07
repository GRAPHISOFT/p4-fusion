#include "fs_path.h"
static int parse_header_path_buf(git_str *path, git_patch_parse_ctx *ctx, size_t path_len)
	if ((error = git_str_put(path, ctx->parse_ctx.line, path_len)) < 0)
	git_str_rtrim(path);
	    (error = git_str_unquote(path)) < 0)
	git_fs_path_squash_slashes(path);
	git_str path = GIT_STR_INIT;
	*out = git_str_detach(&path);
	git_str_dispose(&path);
	git_str old_path = GIT_STR_INIT;
	patch->old_path = git_str_detach(&old_path);
	git_str_dispose(&old_path);
	git_str new_path = GIT_STR_INIT;
	patch->new_path = git_str_detach(&new_path);
	git_str_dispose(&new_path);
	git_str path = GIT_STR_INIT;
	*out = git_str_detach(&path);
	 * proceed here. We then hope for the "---" and "+++" lines to fix that
	STATE_END
	git_str base85 = GIT_STR_INIT, decoded = GIT_STR_INIT;
		if ((error = git_str_decode_base85(
	binary->data = git_str_detach(&decoded);
	git_str_dispose(&base85);
	git_str_dispose(&decoded);
	GIT_ASSERT_ARG(out);
	GIT_ASSERT_ARG(ctx);