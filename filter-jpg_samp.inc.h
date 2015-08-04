#if 0
void filter_jpg(struct media_info const *info, int out_fd, int in_fd)
{
#endif

	size_t			x;
	pix_t const		*in = (void *)(row_buf);
	pix_t			out_row[3 * info->width];
	JSAMPROW		row = (void *)out_row;

	for (x = 0; x < info->width; x += 2) {
		out_row[3*x+0] = in[2*x+0];
		out_row[3*x+1] = in[2*x+1];
		out_row[3*x+2] = in[2*x+3];
		out_row[3*x+3] = in[2*x+2];
		out_row[3*x+4] = in[2*x+1];
		out_row[3*x+5] = in[2*x+3];
	}

	jpeg_write_scanlines(&cinfo, &row, 1);

#if 0
}
#endif
