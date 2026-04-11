#include "zc/scripting/types/genericdata.h"

#include "base/check.h"
#include "components/zasm/defines.h"
#include "zc/scripting/arrays.h"

#include <cstdint>

extern refInfo *ri;
extern int32_t sarg1;
extern int32_t sarg2;
extern int32_t sarg3;

int32_t genericdata_get_register(int32_t reg)
{
	int32_t ret = 0;

	switch (reg)
	{
		case GENDATARUNNING:
		{
			ret = 0;
			if(user_genscript* scr = checkGenericScr(GET_REF(genericdataref)))
			{
				ret = scr->doscript() ? 10000L : 0L;
			}
			break;
		}
		case GENDATASIZE:
		{
			ret = 0;
			if(user_genscript* scr = checkGenericScr(GET_REF(genericdataref)))
			{
				ret = scr->dataSize()*10000;
			}
			break;
		}

		default:
			NOTREACHED();
	}

	return ret;
}

void genericdata_set_register(int32_t reg, int32_t value)
{
	switch (reg)
	{
		case GENDATARUNNING:
		{
			if(user_genscript* scr = checkGenericScr(GET_REF(genericdataref)))
			{
				if(value)
					scr->launch();
				else scr->quit();
			}
			break;
		}
		case GENDATASIZE:
		{
			if(user_genscript* scr = checkGenericScr(GET_REF(genericdataref)))
			{
				scr->dataResize(value/10000);
			}
			break;
		}

		default:
			NOTREACHED();
	}
}

// genericdata arrays.

static ArrayRegistrar GENDATAEVENTSTATE_registrar(GENDATAEVENTSTATE, []{
	static ScriptingArray_ObjectMemberBitwiseFlags<user_genscript, &user_genscript::eventstate, GENSCR_NUMEVENT> impl;
	impl.setMul10000(true);
	return &impl;
}());

static ArrayRegistrar GENDATAEXITSTATE_registrar(GENDATAEXITSTATE, []{
	static ScriptingArray_ObjectMemberBitwiseFlags<user_genscript, &user_genscript::exitState, GENSCR_NUMST> impl;
	impl.setMul10000(true);
	return &impl;
}());

static ArrayRegistrar GENDATARELOADSTATE_registrar(GENDATARELOADSTATE, []{
	static ScriptingArray_ObjectMemberBitwiseFlags<user_genscript, &user_genscript::reloadState, GENSCR_NUMST> impl;
	impl.setMul10000(true);
	return &impl;
}());

static ArrayRegistrar GENDATAINITD_registrar(GENDATAINITD, []{
	static ScriptingArray_ObjectMemberContainer<user_genscript, &user_genscript::initd> impl;
	impl.setMul10000(false);
	return &impl;
}());

static ArrayRegistrar GENDATADATA_registrar(GENDATADATA, []{
	static ScriptingArray_ObjectMemberContainer<user_genscript, &user_genscript::data> impl;
	impl.setMul10000(false);
	return &impl;
}());
