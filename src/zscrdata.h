#ifndef ZSCR_DATA_H
#define ZSCR_DATA_H

#include "base/zdefs.h"
#include "ConsoleLogger.h"

#ifndef IS_PARSER
#include "zquest.h"
#endif //!IS_PARSER

#define ZC_CONSOLE_ERROR_CODE -9997
#define ZC_CONSOLE_WARN_CODE -9996
#define ZC_CONSOLE_INFO_CODE -9998

using std::map;
using std::string;
using std::vector;

namespace ZScript
{
	enum ScriptTypeID
	{
		scrTypeIdInvalid = ZScript::ScriptType::idInvalid,
		scrTypeIdStart,
		scrTypeIdGlobal = scrTypeIdStart,
		scrTypeIdFfc,
		scrTypeIdItem,
		scrTypeIdNPC,
		scrTypeIdEWeapon,
		scrTypeIdLWeapon,
		scrTypeIdPlayer,
		scrTypeIdScreen,
		scrTypeIdDMap,
		scrTypeIdItemSprite,
		scrTypeIdUntyped,
		scrTypeIdComboData,
		scrTypeIdSubscreenData,
		scrTypeIdGeneric,
		
		scrTypeIdEnd
	};
}

void read_compile_data(map<string, ZScript::ScriptTypeID>& stypes, map<string, disassembled_script_data>& scripts)
{
	stypes.clear();
	scripts.clear();
	size_t stypes_sz, scripts_sz;
	size_t dummy;
	ZScript::ScriptTypeID _id;
	char buf[512] = {0};
	char* buf2 = nullptr;
	size_t buf2sz = 0;
	
	FILE *tempfile = fopen("tmp2","rb");
			
	if(!tempfile)
	{
		//jwin_alert("Error","Unable to open the temporary file in current directory!",NULL,NULL,"O&K",NULL,'k',0,lfont);
		return;
	}
	
	fread(&stypes_sz, sizeof(size_t), 1, tempfile);
	for(size_t ind = 0; ind < stypes_sz; ++ind)
	{
		fread(&dummy, sizeof(size_t), 1, tempfile);
		dummy = fread(buf, sizeof(char), dummy, tempfile);
		buf[dummy] = 0;
		fread(&_id, sizeof(ZScript::ScriptTypeID), 1, tempfile);
		stypes[buf] = _id;
	}
	
	fread(&scripts_sz, sizeof(size_t), 1, tempfile);
	for(size_t ind = 0; ind < scripts_sz; ++ind)
	{
		fread(&dummy, sizeof(size_t), 1, tempfile);

		dummy = fread(buf, sizeof(char), dummy, tempfile);
		buf[dummy] = 0;
		
		disassembled_script_data dsd;
		
		fread(&(dsd.first), sizeof(zasm_meta), 1, tempfile);
		
		fread(&(dsd.format), sizeof(byte), 1, tempfile);
		
		size_t tmp;
		fread(&tmp, sizeof(size_t), 1, tempfile);
		for(size_t ind2 = 0; ind2 < tmp; ++ind2)
		{
			fread(&dummy, sizeof(size_t), 1, tempfile);
			if (buf2sz < dummy + 1)
			{
				if (buf2) free(buf2);
				buf2sz = zc_max(dummy + 1, 1024);
				buf2 = (char*)malloc(buf2sz);
				if (!buf2)
				{
					buf2sz = 0;
					goto read_compile_error;
				}
			}
			dummy = fread(buf2, sizeof(char), dummy, tempfile);
			if (dummy >= buf2sz)
			{
				dummy = buf2sz - 1; //This indicates an error, and shouldn't be reached...
			}
			buf2[dummy] = 0;
			int32_t lbl;
			fread(&lbl, sizeof(int32_t), 1, tempfile);
			std::shared_ptr<ZScript::Opcode> oc = std::make_shared<ZScript::ArbitraryOpcode>(buf2);
			oc->setLabel(lbl);
			dsd.second.push_back(oc);
		}
		
		scripts[buf] = dsd;
	}

read_compile_error:
	fclose(tempfile);
	
	if (buf2) free(buf2);
	/*
	reader->read(&stypes_sz, sizeof(size_t));
	for(size_t ind = 0; ind < stypes_sz; ++ind)
	{
		reader->read(&dummy, sizeof(size_t));
		reader->read(buf, dummy, &dummy);
		buf[dummy] = 0;
		reader->read(&_id, sizeof(ZScript::ScriptTypeID));
		stypes[buf] = _id;
	}
	
	reader->read(&scripts_sz, sizeof(size_t));
	for(size_t ind = 0; ind < scripts_sz; ++ind)
	{
		reader->read(&dummy, sizeof(size_t));
		reader->read(buf, dummy, &dummy);
		buf[dummy] = 0;
		
		disassembled_script_data dsd;
		
		reader->read(&(dsd.first), sizeof(zasm_meta));
		
		reader->read(&(dsd.format), sizeof(byte));
		
		size_t tmp;
		reader->read(&tmp, sizeof(size_t));
		for(size_t ind2 = 0; ind2 < tmp; ++ind2)
		{
			reader->read(&dummy, sizeof(size_t));
			reader->read(buf2, dummy, &dummy);
			buf2[dummy] = 0;
			int32_t lbl;
			reader->read(&lbl, sizeof(int32_t));
			std::shared_ptr<ZScript::Opcode> oc = std::make_shared<ZScript::ArbitraryOpcode>(string(buf2));
			oc->setLabel(lbl);
			dsd.second.push_back(oc);
		}
		
		scripts[buf] = dsd;
	}
	*/
}

void write_compile_data(map<string, ZScript::ScriptTypeID>& stypes, map<string, disassembled_script_data>& scripts)
{
	size_t dummy = stypes.size();
	FILE *tempfile = fopen("tmp2","wb");
			
	if(!tempfile)
	{
		//jwin_alert("Error","Unable to create a temporary file in current directory!",NULL,NULL,"O&K",NULL,'k',0,lfont);
		return;
	}
	
	fwrite(&dummy, sizeof(size_t), 1, tempfile);
	for(auto it = stypes.begin(); it != stypes.end(); ++it)
	{
		string const& str = it->first;
		ZScript::ScriptTypeID v = it->second;
		dummy = str.size();
		fwrite(&dummy, sizeof(size_t), 1, tempfile);
		fwrite((void*)str.c_str(), sizeof(char), dummy, tempfile);
		fwrite(&v, sizeof(ZScript::ScriptTypeID), 1, tempfile);
	}
	
	dummy = scripts.size();
	fwrite(&dummy, sizeof(size_t), 1, tempfile);
	for(auto it = scripts.begin(); it != scripts.end(); ++it)
	{
		string const& str = it->first;
		disassembled_script_data& v = it->second;
		dummy = str.size();
		fwrite(&dummy, sizeof(size_t), 1, tempfile);
		fwrite((void*)str.c_str(), sizeof(char), dummy, tempfile);
		
		fwrite(&(v.first), sizeof(zasm_meta), 1, tempfile);
		
		fwrite(&(v.format), sizeof(byte), 1, tempfile);
		
		dummy = v.second.size();
		fwrite(&dummy, sizeof(size_t), 1, tempfile);
		
		for(auto it = v.second.begin(); it != v.second.end(); ++it)
		{
			string opstr = (*it)->toString();
			int32_t lbl = (*it)->getLabel();
			
			dummy = opstr.size();
			fwrite(&dummy, sizeof(size_t), 1, tempfile);
			fwrite((void*)opstr.c_str(), sizeof(char), dummy, tempfile);
			
			fwrite(&lbl, sizeof(int32_t), 1, tempfile);
		}
	}
	
	//fwrite(zScript.c_str(), sizeof(char), zScript.size(), tempfile);
	fclose(tempfile);
	/*
	
	writer->write(&dummy, sizeof(size_t));
	for(auto it = stypes.begin(); it != stypes.end(); ++it)
	{
		string const& str = it->first;
		ZScript::ScriptTypeID v = it->second;
		dummy = str.size();
		writer->write(&dummy, sizeof(size_t));
		writer->write((void*)str.c_str(), dummy);
		writer->write(&v, sizeof(ZScript::ScriptTypeID));
	}
	
	dummy = scripts.size();
	writer->write(&dummy, sizeof(size_t));
	for(auto it = scripts.begin(); it != scripts.end(); ++it)
	{
		string const& str = it->first;
		disassembled_script_data& v = it->second;
		dummy = str.size();
		writer->write(&dummy, sizeof(size_t));
		writer->write((void*)str.c_str(), dummy);
		
		writer->write(&(v.first), sizeof(zasm_meta));
		
		writer->write(&(v.format), sizeof(byte));
		
		dummy = v.second.size();
		writer->write(&dummy, sizeof(size_t));
		
		for(auto it = v.second.begin(); it != v.second.end(); ++it)
		{
			string opstr = (*it)->toString();
			int32_t lbl = (*it)->getLabel();
			
			dummy = opstr.size();
			writer->write(&dummy, sizeof(size_t));
			writer->write((void*)opstr.c_str(), dummy);
			
			writer->write(&lbl, sizeof(int32_t));
		}
	}
	*/
}

#ifndef IS_PARSER

CConsoleLoggerEx parser_console;

static const int32_t WARN_COLOR = CConsoleLoggerEx::COLOR_RED | CConsoleLoggerEx::COLOR_GREEN;
static const int32_t ERR_COLOR = CConsoleLoggerEx::COLOR_RED;
static const int32_t INFO_COLOR = CConsoleLoggerEx::COLOR_WHITE;

void zconsole_warn2(const char *format,...)
{
	if (!DisableCompileConsole)
	{
		int32_t v = parser_console.cprintf( WARN_COLOR, "[Warn] ");
		if(v < 0) return; //Failed to print
	}
	//{
	int32_t ret;
	char tmp[1024];
	
	va_list argList;
	va_start(argList, format);
	#ifdef WIN32
	 		ret = _vsnprintf(tmp,sizeof(tmp)-1,format,argList);
	#else
	 		ret = vsnprintf(tmp,sizeof(tmp)-1,format,argList);
	#endif
	tmp[vbound(ret,0,1023)]=0;
	
	va_end(argList);
	//}
	al_trace("%s\n", tmp);
	if (!DisableCompileConsole) parser_console.cprintf( WARN_COLOR, "%s\n", tmp);
	else
	{
		box_out(tmp);
		box_eol();
	}
}
void zconsole_error2(const char *format,...)
{
	if (!DisableCompileConsole)
	{
		int32_t v = parser_console.cprintf( ERR_COLOR,"[Error] ");
		if(v < 0) return; //Failed to print
	}
	//{
	int32_t ret;
	char tmp[1024];
	
	va_list argList;
	va_start(argList, format);
	#ifdef WIN32
	 		ret = _vsnprintf(tmp,sizeof(tmp)-1,format,argList);
	#else
	 		ret = vsnprintf(tmp,sizeof(tmp)-1,format,argList);
	#endif
	tmp[vbound(ret,0,1023)]=0;
	
	va_end(argList);
	//}
	al_trace("%s\n", tmp);
	if (!DisableCompileConsole) parser_console.cprintf( ERR_COLOR, "%s\n", tmp);
	else
	{
		box_out(tmp);
		box_eol();
	}
}
void zconsole_info2(const char *format,...)
{
	if (!DisableCompileConsole)
	{
		int32_t v = parser_console.cprintf( INFO_COLOR,"[Info] ");
		if(v < 0) return; //Failed to print
	}
	//{
	int32_t ret;
	char tmp[1024];
	
	va_list argList;
	va_start(argList, format);
	#ifdef WIN32
	 		ret = _vsnprintf(tmp,sizeof(tmp)-1,format,argList);
	#else
	 		ret = vsnprintf(tmp,sizeof(tmp)-1,format,argList);
	#endif
	tmp[vbound(ret,0,1023)]=0;
	
	va_end(argList);
	//}
	al_trace("%s\n", tmp);
	if (!DisableCompileConsole) parser_console.cprintf( INFO_COLOR, "%s\n", tmp);
	else
	{
		box_out(tmp);
		box_eol();
	}
}

void ReadConsole(char buf[], int code)
{
	//al_trace("%s\n", buf);
	switch(code)
	{
		case ZC_CONSOLE_WARN_CODE: zconsole_warn2("%s", buf); break;
		case ZC_CONSOLE_ERROR_CODE: zconsole_error2("%s", buf); break;
		default: zconsole_info2("%s", buf); break;
	}
}
#endif //!IS_PARSER

#ifdef IS_PARSER
#include "parser/Compiler.h"
void write_compile_data(map<string, ZScript::ScriptType>& stypes, map<string, disassembled_script_data>& scripts)
{
	map<string, ZScript::ScriptTypeID> sid_types;
	for(auto it = stypes.begin(); it != stypes.end(); ++it)
	{
		sid_types[it->first] = (ZScript::ScriptTypeID)(it->second.getId());
	}
	write_compile_data(sid_types, scripts);
}

#endif //IS_PARSER

#endif

