/*
 * Thanks to lloyd for contribution
 */

extern char concat[8192];

extern void
ccat(const unsigned short int count, ...)
{
	va_list ap;
	unsigned short int i;
	concat[0] = '\0';

	va_start(ap, count);
	for(i = 0; i < count; i++)
		strlcat(concat, va_arg(ap, char *), sizeof(concat));
	va_end(ap);
	return;
}
