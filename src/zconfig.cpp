#include "zc_alleg.h"
#include "zconfig.h"

void zc_set_config_standard()
{
	flush_config_file();
	set_config_file(STANDARD_CFG);
}

bool zc_config_standard_exists()
{
	return exists(STANDARD_CFG);
}

static char cfg_str[2048];

int32_t zc_get_config(char const* header, char const* name, int32_t default_val)
{
	int32_t ret = get_config_int(header,name,default_val);
	if(ret == default_val) //Might have been defaulted, so write it back
		set_config_int(header, name, ret);
	return ret;
}
double zc_get_config(char const* header, char const* name, double default_val)
{
	double ret = get_config_float(header,name,default_val);
	if(ret == default_val) //Might have been defaulted, so write it back
		set_config_float(header, name, ret);
	return ret;
}
char const* zc_get_config(char const* header, char const* name, char const* default_val)
{
	char const* ret = get_config_string(header,name,default_val);
	if(ret==default_val) //Defaulted, so write it back
	{
		if (!default_val)
			set_config_string(header, name, "");
		else if(default_val[0]) //Writing back the empty string destroys the value?? -Em
			set_config_string(header, name, default_val);
	}
	if(!ret)
		cfg_str[0] = 0;
	else strcpy(cfg_str, ret);
	return cfg_str;
}
void zc_set_config(char const* header, char const* name, int32_t val)
{
	set_config_int(header,name,val);
}
void zc_set_config(char const* header, char const* name, double default_val)
{
	set_config_float(header, name, default_val);
}
void zc_set_config(char const* header, char const* name, char const* val)
{
	set_config_string(header,name,val);
}

int32_t zc_get_config(char const* cfg_file, char const* header, char const* name, int32_t default_val)
{
	push_config_state();
	set_config_file(cfg_file);
	int32_t ret = get_config_int(header,name,default_val);
	if(ret == default_val) //Might have been defaulted, so write it back
		set_config_int(header, name, ret);
	pop_config_state();
	return ret;
}
double zc_get_config(char const* cfg_file, char const* header, char const* name, double default_val)
{
	push_config_state();
	set_config_file(cfg_file);
	double ret = get_config_float(header,name,default_val);
	if(ret == default_val) //Might have been defaulted, so write it back
		set_config_float(header, name, ret);
	pop_config_state();
	return ret;
}
char const* zc_get_config(char const* cfg_file, char const* header, char const* name, char const* default_val)
{
	push_config_state();
	set_config_file(cfg_file);
	char const* ret = get_config_string(header,name,default_val);
	if(ret==default_val) //Defaulted, so write it back
	{
		if(default_val[0]) //Writing back the empty string destroys the value?? -Em
			set_config_string(header, name, default_val);
	}
	strcpy(cfg_str, ret);
	pop_config_state();
	return cfg_str;
}
void zc_set_config(char const* cfg_file, char const* header, char const* name, int32_t val)
{
	push_config_state();
	set_config_file(cfg_file);
	set_config_int(header,name,val);
	pop_config_state();
}
void zc_set_config(char const* cfg_file, char const* header, char const* name, double default_val)
{
	push_config_state();
	set_config_file(cfg_file);
	set_config_float(header, name, default_val);
	pop_config_state();
}
void zc_set_config(char const* cfg_file, char const* header, char const* name, char const* val)
{
	push_config_state();
	set_config_file(cfg_file);
	set_config_string(header,name,val);
	pop_config_state();
}

