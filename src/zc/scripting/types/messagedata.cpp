#include "zc/scripting/types/messagedata.h"

#include "base/check.h"
#include "components/zasm/defines.h"
#include "zc/scripting/arrays.h"

extern refInfo *ri;
extern int32_t sarg1;
extern int32_t sarg2;
extern int32_t sarg3;

// TODO !
int32_t do_msgheight(int32_t msg);
int32_t do_msgwidth(int32_t msg);

int32_t messagedata_get_register(int32_t reg)
{
	int32_t ret = 0;

	switch (reg)
	{
		case MESSAGEDATACSET: //b
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				ret = -10000;
			else 
				ret = ((int32_t)MsgStrings[ID].cset) * 10000;
			break;
		}	
		case MESSAGEDATAFLAGS: //BYTE
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				ret = -10000;
			else 
				ret = ((int32_t)MsgStrings[ID].stringflags) * 10000;
			break;
		}
		case MESSAGEDATAFONT: //B
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				ret = -10000;
			else 
				ret = (int32_t)MsgStrings[ID].font * 10000;
			break;
		}	
		case MESSAGEDATAH: //UNSIGNED SHORT
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				ret = -10000;
			else 
				ret = ((int32_t)MsgStrings[ID].h) * 10000;
			break;
		}	
		case MESSAGEDATAHSPACE: //BYTE
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				ret = -10000;
			else 
				ret = ((int32_t)MsgStrings[ID].hspace) * 10000;
			break;
		}	
		case MESSAGEDATALISTPOS: //WORD
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				ret = -10000;
			else 
				ret = ((int32_t)MsgStrings[ID].listpos) * 10000;
			break;
		}	
		case MESSAGEDATANEXT: //W
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
			{
				ret = -10000; break;
			}
			else 
			{
				ret = ((int32_t)MsgStrings[ID].nextstring) * 10000;
				break;
			}
		}	
		case MESSAGEDATAPORTCSET: //BYTE
		{
			int32_t ID = GET_REF(msgdataref);

			if(BC::checkMessage(ID) != SH::_NoError)
				ret = -10000;
			else
				ret = ((int32_t)MsgStrings[ID].portrait_cset) * 10000;
			break;
		}
		case MESSAGEDATAPORTHEI: //BYTE
		{
			int32_t ID = GET_REF(msgdataref);

			if(BC::checkMessage(ID) != SH::_NoError)
				ret = -10000;
			else
				ret = ((int32_t)MsgStrings[ID].portrait_th) * 10000;
			break;
		}
		case MESSAGEDATAPORTTILE: //INT
		{
			int32_t ID = GET_REF(msgdataref);

			if(BC::checkMessage(ID) != SH::_NoError)
				ret = -10000;
			else
				ret = ((int32_t)MsgStrings[ID].portrait_tile) * 10000;
			break;
		}
		case MESSAGEDATAPORTWID: //BYTE
		{
			int32_t ID = GET_REF(msgdataref);

			if(BC::checkMessage(ID) != SH::_NoError)
				ret = -10000;
			else
				ret = ((int32_t)MsgStrings[ID].portrait_tw) * 10000;
			break;
		}
		case MESSAGEDATAPORTX: //BYTE
		{
			int32_t ID = GET_REF(msgdataref);

			if(BC::checkMessage(ID) != SH::_NoError)
				ret = -10000;
			else
				ret = ((int32_t)MsgStrings[ID].portrait_x) * 10000;
			break;
		}
		case MESSAGEDATAPORTY: //BYTE
		{
			int32_t ID = GET_REF(msgdataref);

			if(BC::checkMessage(ID) != SH::_NoError)
				ret = -10000;
			else
				ret = ((int32_t)MsgStrings[ID].portrait_y) * 10000;
			break;
		}
		case MESSAGEDATASFX: //BYTE
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				ret = -10000;
			else 
				ret = ((int32_t)MsgStrings[ID].sfx) * 10000;
			break;
		}	
		case MESSAGEDATATEXTHEI:
		{
			ret = do_msgheight(GET_REF(msgdataref))*10000;
			break;
		}

		///----------------------------------------------------------------------------------------------------//
		//combodata cd-> Getter variables
		#define	GET_COMBO_VAR_INT(member) \
		{ \
			if(!checkComboRef()) \
			{ \
				ret = -10000; \
			} \
			else \
			{ \
				ret = (combobuf[GET_REF(combodataref)].member *10000); \
			} \
		} \

		#define	GET_COMBO_VAR_BYTE(member) \
		{ \
			if(!checkComboRef()) \
			{ \
				ret = -10000; \
			} \
			else \
			{ \
				ret = (combobuf[GET_REF(combodataref)].member *10000); \
			} \
		} \

		#define	GET_COMBO_VAR_DWORD(member) \
		{ \
			if(!checkComboRef()) \
			{ \
				ret = -10000; \
			} \
			else \
			{ \
				ret = (combobuf[GET_REF(combodataref)].member *10000); \
			} \
		} \

		//comboclass macros

		#define	GET_COMBOCLASS_VAR_INT(member) \
		{ \
			if(!checkComboRef()) \
			{ \
				ret = -10000; \
			} \
			else \
			{ \
				ret = (combo_class_buf[combobuf[GET_REF(combodataref)].type].member *10000); \
			} \
		} \

		#define	GET_COMBOCLASS_VAR_BYTE(member) \
		{ \
			if(!checkComboRef()) \
			{ \
				ret = -10000; \
			} \
			else \
			{ \
				ret = (combo_class_buf[combobuf[GET_REF(combodataref)].type].member *10000); \
			} \
		} \

		#define	GET_COMBOCLASS_VAR_DWORD(member) \
		{ \
			if(!checkComboRef()) \
			{ \
				ret = -10000; \
			} \
			else \
			{ \
				ret = (combo_class_buf[combobuf[GET_REF(combodataref)].type].member *10000); \
			} \
		} \

		#define GET_COMBOCLASS_BYTE_INDEX(member, indexbound) \
		{ \
				int32_t indx = GET_D(rINDEX) / 10000; \
				if(!checkComboRef()) \
				{ \
					ret = -10000; \
				} \
				else if ( indx < 0 || indx > indexbound ) \
				{ \
					scripting_log_error_with_context("Invalid Array Index: {}", indx); \
					ret = -10000; \
				} \
				else \
				{ \
					ret = (combo_class_buf[combobuf[GET_REF(combodataref)].type].member[indx] * 100000); \
				} \
		}
		case MESSAGEDATATEXTLEN: //BYTE
		{
			int32_t ID = GET_REF(msgdataref);

			if(BC::checkMessage(ID) != SH::_NoError)
				ret = -10000;
			else
				ret = int32_t(MsgStrings[ID].s.size()) * 10000;
			break;
		}
		case MESSAGEDATATEXTWID:
		{
			ret = do_msgwidth(GET_REF(msgdataref))*10000;
			break;
		}
		case MESSAGEDATATILE: //W
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				ret = -10000;
			else 
				ret = ((int32_t)MsgStrings[ID].tile) * 10000;
			break;
		}	
		case MESSAGEDATATRANS: //BOOL
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				ret = -10000;
			else 
				ret = ((MsgStrings[ID].trans)?10000:0);
			break;
		}	
		case MESSAGEDATAVSPACE: //BYTE
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				ret = -10000;
			else 
				ret = ((int32_t)MsgStrings[ID].vspace) * 10000;
			break;
		}	
		case MESSAGEDATAW: //UNSIGNED SHORT
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				ret = -10000;
			else 
				ret = ((int32_t)MsgStrings[ID].w) * 10000;
			break;
		}	
		case MESSAGEDATAX: //SHORT
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				ret = -10000;
			else 
				ret = ((int32_t)MsgStrings[ID].x) * 10000;
			break;
		}	
		case MESSAGEDATAY: //SHORT
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				ret = -10000;
			else 
				ret = ((int32_t)MsgStrings[ID].y) * 10000;
			break;
		}

		default:
			NOTREACHED();
	}

	return ret;
}

void messagedata_set_register(int32_t reg, int32_t value)
{
	switch (reg)
	{
		case MESSAGEDATACSET: //b
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				break;
			else 
				MsgStrings[ID].cset = ((byte)vbound((value/10000), 0, 15));
			break;
		}	
		case MESSAGEDATAFLAGS: //BYTE
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				break;
			else 
				MsgStrings[ID].stringflags = ((byte)vbound((value/10000), 0, 255));
			break;
		}
		case MESSAGEDATAFONT: //B
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				break;
			else 
				MsgStrings[ID].font = ((byte)vbound((value/10000), 0, 255));
			break;
		}	
		case MESSAGEDATAH: //UNSIGNED SHORT
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				break;
			else 
				MsgStrings[ID].h = ((uint16_t)vbound((value/10000), 0, USHRT_MAX));
			break;
		}	
		case MESSAGEDATAHSPACE: //BYTE
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				break;
			else 
				MsgStrings[ID].hspace = ((byte)vbound((value/10000), 0, 255));
			break;
		}	
		case MESSAGEDATALISTPOS: //WORD
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				break;
			else 
				MsgStrings[ID].listpos = vbound((value/10000), 1, (msg_count-1));
			break;
		}	
		case MESSAGEDATANEXT: //W
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				break;
			else 
				MsgStrings[ID].nextstring = vbound((value/10000), 0, (msg_count-1));
			break;
		}	
		case MESSAGEDATAPORTCSET: //BYTE
		{
			int32_t ID = GET_REF(msgdataref);

			if(BC::checkMessage(ID) != SH::_NoError)
				break;
			else
				MsgStrings[ID].portrait_cset = ((byte)vbound((value/10000), 0, 15));
			break;
		}
		case MESSAGEDATAPORTHEI: //BYTE
		{
			int32_t ID = GET_REF(msgdataref);

			if(BC::checkMessage(ID) != SH::_NoError)
				break;
			else
				MsgStrings[ID].portrait_th = ((byte)vbound((value/10000), 0, 14));
			break;
		}

		///----------------------------------------------------------------------------------------------------//
		//combodata cd-> Setter Variables
		//newcombo	
		#define	SET_COMBO_VAR_INT(member) \
		{ \
			if(checkComboRef()) \
			{ \
				screen_combo_modify_pre(GET_REF(combodataref)); \
				combobuf[GET_REF(combodataref)].member = vbound((value / 10000),0,214747); \
				screen_combo_modify_post(GET_REF(combodataref)); \
				\
			} \
		} \

		#define	SET_COMBO_VAR_DWORD(member) \
		{ \
			if(checkComboRef()) \
			{ \
				screen_combo_modify_pre(GET_REF(combodataref)); \
				combobuf[GET_REF(combodataref)].member = vbound((value / 10000),0,32767); \
				screen_combo_modify_post(GET_REF(combodataref)); \
			} \
		} \

		#define	SET_COMBO_VAR_BYTE(member) \
		{ \
			if(checkComboRef()) \
			{ \
				screen_combo_modify_pre(GET_REF(combodataref)); \
				combobuf[GET_REF(combodataref)].member = vbound((value / 10000),0,255); \
				screen_combo_modify_post(GET_REF(combodataref)); \
			} \
		} \

		//comboclass
		#define	SET_COMBOCLASS_VAR_INT(member) \
		{ \
			if(checkComboRef()) \
			{ \
				combo_class_buf[combobuf[GET_REF(combodataref)].type].member = vbound((value / 10000),0,214747); \
			} \
		} \

		#define	SET_COMBOCLASS_VAR_DWORD(member) \
		{ \
			if(checkComboRef()) \
			{ \
				combo_class_buf[combobuf[GET_REF(combodataref)].type].member = vbound((value / 10000),0,32767); \
			} \
		} \

		#define	SET_COMBOCLASS_VAR_BYTE(member) \
		{ \
			if(checkComboRef()) \
			{ \
				combo_class_buf[combobuf[GET_REF(combodataref)].type].member = vbound((value / 10000),0,255); \
			} \
		} \

		#define SET_COMBOCLASS_BYTE_INDEX(member, indexbound) \
		{ \
				int32_t indx = GET_D(rINDEX) / 10000; \
				if(!checkComboRef()) \
				{ \
				} \
				else if ( indx < 0 || indx > indexbound ) \
				{ \
					scripting_log_error_with_context("Invalid Array Index: {}", indx); \
				} \
				else \
				{ \
					combo_class_buf[combobuf[GET_REF(combodataref)].type].member[indx] = vbound((value / 10000),0,255); \
				} \
		}
		case MESSAGEDATAPORTTILE: //INT
		{
			int32_t ID = GET_REF(msgdataref);

			if(BC::checkMessage(ID) != SH::_NoError)
				break;
			else
				MsgStrings[ID].portrait_tile = vbound((value/10000), 0, (NEWMAXTILES));
			break;
		}
		case MESSAGEDATAPORTWID: //BYTE
		{
			int32_t ID = GET_REF(msgdataref);

			if(BC::checkMessage(ID) != SH::_NoError)
				break;
			else
				MsgStrings[ID].portrait_tw = ((byte)vbound((value/10000), 0, 16));
			break;
		}
		case MESSAGEDATAPORTX: //BYTE
		{
			int32_t ID = GET_REF(msgdataref);

			if(BC::checkMessage(ID) != SH::_NoError)
				break;
			else
				MsgStrings[ID].portrait_x = ((byte)vbound((value/10000), 0, 255));
			break;
		}
		case MESSAGEDATAPORTY: //BYTE
		{
			int32_t ID = GET_REF(msgdataref);

			if(BC::checkMessage(ID) != SH::_NoError)
				break;
			else
				MsgStrings[ID].portrait_y = ((byte)vbound((value/10000), 0, 255));
			break;
		}
		case MESSAGEDATASFX: //BYTE
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				break;
			else 
				MsgStrings[ID].sfx = ((word)vbound((value/10000), 0, MAX_SFX));
			break;
		}	
		case MESSAGEDATATILE: //W
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				break;
			else 
				MsgStrings[ID].tile = vbound((value/10000), 0, (NEWMAXTILES));
			break;
		}	
		case MESSAGEDATATRANS: //BOOL
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				break;
			else 
				(MsgStrings[ID].trans) = ((value)?true:false);
			break;
		}	
		case MESSAGEDATAVSPACE: //BYTE
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				break;
			else 
				MsgStrings[ID].vspace = ((byte)vbound((value/10000), 0, 255));
			break;
		}	
		case MESSAGEDATAW: //UNSIGNED SHORT
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				break;
			else 
				MsgStrings[ID].w = ((uint16_t)vbound((value/10000), 0, USHRT_MAX));
			break;
		}	
		case MESSAGEDATAX: //SHORT
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				break;
			else 
				MsgStrings[ID].x = ((int16_t)vbound((value/10000), SHRT_MIN, SHRT_MAX));
			break;
		}	
		case MESSAGEDATAY: //SHORT
		{
			int32_t ID = GET_REF(msgdataref);	

			if(BC::checkMessage(ID) != SH::_NoError)
				break;
			else 
				MsgStrings[ID].y = ((int16_t)vbound((value/10000), SHRT_MIN, SHRT_MAX));
			break;
		}

		default:
			NOTREACHED();
	}
}

// messagedata arrays.

static ArrayRegistrar MESSAGEDATAMARGINS_registrar(MESSAGEDATAMARGINS, []{
	static ScriptingArray_ObjectMemberCArray<MsgStr, &MsgStr::margins> impl;
	impl.compatSetDefaultValue(-10000);
	impl.setMul10000(true);
	impl.setValueTransform(transforms::vboundByte);
	return &impl;
}());

static ArrayRegistrar MESSAGEDATAFLAGSARR_registrar(MESSAGEDATAFLAGSARR, []{
	static ScriptingArray_ObjectMemberBitwiseFlags<MsgStr, &MsgStr::stringflags, 7> impl;
	impl.setMul10000(true);
	return &impl;
}());

static ArrayRegistrar MESSAGEDATASEGMENTS_registrar(MESSAGEDATASEGMENTS, []{
	static ScriptingArray_ObjectComputed<MsgStr, int32_t> impl(
		[](MsgStr* msg){ return msg->segmentsAsIntArray().size(); },
		[](MsgStr* msg, int index){ return msg->segmentsAsIntArray()[index]; },
		[](MsgStr*, int, int){}
	);
	impl.setMul10000(true);
	impl.setReadOnly();
	return &impl;
}());
