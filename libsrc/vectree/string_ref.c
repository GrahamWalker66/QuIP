#include "quip_config.h"
#include "quip_prot.h"
#include "string_ref.h"

String_Ref *save_stringref(const char *s)
{
	String_Ref *srp;

	srp = getbuf( sizeof(String_Ref) );
	srp->sr_string = savestr(s);
	srp->sr_count = 0;

	return srp;
}

void rls_stringref( String_Ref *srp )
{
	rls_str(srp->sr_string);
	givbuf(srp);
}

