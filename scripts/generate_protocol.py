# python -m pip install pyyaml
#
# Generates C++ bindings based on protocol defined in `protocol.yml`
# Outputs to src/zq/protocol

import os
import textwrap
from pathlib import Path
from dataclasses import dataclass
from typing import List, Any, Optional, Union, OrderedDict
import yaml

script_dir = Path(os.path.dirname(os.path.realpath(__file__)))
root_dir = script_dir.parent
protocol_yml_path = root_dir / 'src/zq/protocol/protocol.yml'
with open(protocol_yml_path, 'r') as stream:
	protocol_config_yml = yaml.safe_load(stream)

builtin_types = ['string', 'int', 'bool']
custom_types = []
custom_types_registry = {}

@dataclass
class Type:
	klass: str
	array: bool
	default_cpp: Optional[str]
	enumerated_values: List[str]

	def is_custom(self):
		return self.klass in custom_types

	def cpp_type(self):
		if self.klass == 'string':
			cpp_type = 'std::string'
		elif self.klass == 'int':
			cpp_type = 'int'
		elif self.klass == 'bool':
			cpp_type = 'bool'
		elif self.klass in builtin_types:
			cpp_type = self.klass
		elif self.is_custom():
			cpp_type = f'protocol::types::{self.klass}'
		if self.array:
			return f'std::vector<{cpp_type}>'
		return cpp_type


@dataclass
class Command:
	method: str
	params: Optional[OrderedDict[str, Type]]
	result: Optional[OrderedDict[str, Type]]

@dataclass
class Event:
	method: str
	params: Optional[OrderedDict[str, Type]]

@dataclass
class CustomType:
	name: str
	props: Optional[OrderedDict[str, Type]]

@dataclass
class ProtocolConfig:
    types: List[CustomType]
    commands: List[Command]
    events: List[Any]


DescriptorList = Union[List[Command], List[Event]]


def parse_type(type_yml):
	default = None
	enumerated_values = None
	if isinstance(type_yml, str):
		type_str = type_yml
	elif isinstance(type_yml, list):
		type_str = 'string'
		enumerated_values = type_yml
	elif 'type' in type_yml:
		type_str = type_yml['type']
		default = type_yml.get('default', None)
	else:
		type_str = 'string'
		raise Exception(f'unexpected type format: {type_yml}')

	array = type_str.endswith('[]')
	if array:
		type_str = type_str.replace('[]', '')
	klass = type_str
	if klass not in builtin_types and klass not in custom_types:
		return None

	default_cpp = None
	if isinstance(default, bool):
		default_cpp = 'true' if default else 'false'
	elif default != None:
		default_cpp = default
		
	return Type(klass, array, default_cpp, enumerated_values)


def parse_type_object(type_yml):
	if not type_yml:
		return None

	type_obj = {}
	for name, type_yml in type_yml.items():
		type = parse_type(type_yml)
		if not type:
			raise Exception(f'invalid type {type_yml} given for param {name}')
		type_obj[name] = type
	return type_obj


def parse_protocol_config(config_yml):
	types = []
	for name, type_yml in config_yml['types'].items():
		type_obj = parse_type_object(type_yml)
		custom_types.append(name)
		custom_type = CustomType(name, type_obj)
		custom_types_registry[name] = custom_type
		types.append(custom_type)

	commands = []
	for method, command_yml in config_yml['commands'].items():
		if not command_yml:
			command_yml = {}

		params = parse_type_object(command_yml.get('params', None))
		result = parse_type_object(command_yml.get('result', None))
		commands.append(Command(method, params, result))

	events = []
	for method, event_yml in config_yml['events'].items():
		if not event_yml:
			event_yml = {}

		params = parse_type_object(event_yml.get('params', None))
		events.append(Event(method, params))

	return ProtocolConfig(types, commands, events)

def add(lines: List[str], indent: int, line: str):
	if not line:
		lines.append('')
	else:
		lines.append('\t'*indent + line)

def generate_types_header_section(types: List[CustomType]):
	lines = []
	indent = 0

	add(lines, indent, 'namespace protocol {')
	add(lines, indent, 'namespace types {')
	indent += 1

	for custom_type in types:
		add(lines, indent, f'struct {custom_type.name} {{')
		indent += 1
		for prop, type in custom_type.props.items():
			add(lines, indent, f'{type.cpp_type()} {prop};')
		indent -= 1
		add(lines, indent, '};')

	indent -= 1
	add(lines, indent, '}')
	add(lines, indent, '}')
	add(lines, indent, '')

	return '\n'.join(lines)

def generate_header_section(name: str, descriptors: DescriptorList):
	is_command = name == 'commands'
	lines = []
	indent = 0

	add(lines, indent, 'namespace protocol {')
	add(lines, indent, f'namespace {name} {{')
	indent += 1

	add(lines, indent, 'enum class type {')
	indent += 1
	add(lines, indent, 'none,')
	for descriptor in descriptors:
		add(lines, indent, f'{descriptor.method},')
	indent -= 1
	add(lines, indent, '};\n')

	for descriptor in descriptors:
		name = descriptor.method
		has_params = descriptor.params != None
		has_result = is_command and descriptor.result != None

		add(lines, indent, f'namespace {name} {{')
		indent += 1

		add(lines, indent, 'struct params {')
		if has_params:
			indent += 1
			for name, type in descriptor.params.items():
				add(lines, indent, f'{type.cpp_type()} {name};')
			indent -= 1
		add(lines, indent, '};')

		if is_command:
			add(lines, indent, 'struct result {')
			if has_result:
				indent += 1
				for name, type in descriptor.result.items():
					add(lines, indent, f'{type.cpp_type()} {name};')
				indent -= 1
			add(lines, indent, '};')

			add(lines, indent, 'result handle(params params);')
		else:
			add(lines, indent, 'void emit(params params);')

		indent -= 1
		add(lines, indent, '}\n')

	indent -= 1
	add(lines, indent, '}')
	add(lines, indent, '}')
	return '\n'.join(lines)


def generate_type_parser(name: str, descriptors: DescriptorList):
	lines = []
	indent = 0
	add(lines, indent, f'protocol::{name}s::type protocol_parse_command(std::string {name})')
	add(lines, indent, '{')
	indent += 1

	for descriptor in descriptors:
		add(lines, indent, f'if ({name} == "{descriptor.method}") return protocol::{name}s::type::{descriptor.method};')
	add(lines, indent, f'return protocol::{name}s::type::none;')

	indent -= 1
	add(lines, indent, '}')
	add(lines, indent, '')

	return '\n'.join(lines)

def generate_type_to_string(name: str, descriptors: DescriptorList):
	lines = []
	indent = 0
	add(lines, indent, f'std::string protocol_{name}_to_string(protocol::{name}s::type type)')
	add(lines, indent, '{')
	indent += 1

	for descriptor in descriptors:
		add(lines, indent, f'if (type == protocol::{name}s::type::{descriptor.method}) return "{descriptor.method}";')
	add(lines, indent, f'return "unknown";')

	indent -= 1
	add(lines, indent, '}')
	add(lines, indent, '')

	return '\n'.join(lines)


def generate_read_json_type_fn(type: str):
	if type == 'string':
		default = '""'
		check_fn = 'IsString'
		convert_fn = 'ToString'
	elif type == 'int':
		default = '0'
		check_fn = 'IsIntegral'
		convert_fn = 'ToInt'
	elif type == 'bool':
		default = 'false'
		check_fn = 'IsBoolean'
		convert_fn = 'ToBool'
	else:
		raise Exception(f'invalid type: {type}')

	cpp_type = parse_type(type).cpp_type()

	lines = []
	indent = 0
	add(lines, indent, f'static {cpp_type} read_{type}(JSON& json_object, std::string param, std::optional<{cpp_type}> default_value, std::string& error)')
	add(lines, indent, '{')
	indent += 1

	add(lines, indent, 'const auto& json_value = json_object[param];')

	add(lines, indent, 'if (json_value.IsNull())')
	add(lines, indent, '{')
	indent += 1
	add(lines, indent, 'if (default_value) return *default_value;')
	add(lines, indent, 'error = fmt::format("Value missing for required param {}", param);')
	add(lines, indent, f'return {default};')
	indent -= 1
	add(lines, indent, '}')

	add(lines, indent, f'if (!json_value.{check_fn}())')
	add(lines, indent, '{')
	indent += 1
	add(lines, indent, f'error = fmt::format("Value found for param {{}}, but not of expected type {{}}", param, "{type}");')
	add(lines, indent, f'return {default};')
	indent -= 1
	add(lines, indent, '}')

	add(lines, indent, f'return json_value.{convert_fn}();')

	indent -= 1
	add(lines, indent, '}')
	add(lines, indent, '')
	return '\n'.join(lines)

def generate_write_json_type_fn(type_str: str):
	type = parse_type(type_str)
	cpp_type = type.cpp_type()

	lines = []
	indent = 0
	add(lines, indent, f'static void write_type(JSON& json, const {cpp_type} value)')
	add(lines, indent, '{')
	indent += 1

	if type.is_custom():
		custom_type: CustomType = custom_types_registry[type.klass]
		add(lines, indent, 'json = JSON::Make(JSON::Class::Object);')
		for arg, prop in custom_type.props.items():
			if prop.is_custom() or prop.array:
				add(lines, indent, f'write_type(json["{arg}"], value.{arg});')
			else:
				add(lines, indent, f'json["{arg}"] = value.{arg};')
	else:
		add(lines, indent, 'json = value;')

	indent -= 1
	add(lines, indent, '}')
	add(lines, indent, '')
	return '\n'.join(lines)

def generate_write_json_array_type_fn():
	return """
template <typename T>
static void write_type(JSON& json, const std::vector<T>& values)
{
	json = JSON::Make(JSON::Class::Array);
	json[values.size() - 1] = 0;
	for (size_t i = 0; i < values.size(); i++)
		write_type(json[i], values[i]);
}
"""

def generate_handle_command_fn(descriptors: DescriptorList):
	def generate_read_json_type_call(indent: int, name: str, type: Type):
		if type.array:
			# TODO implement
			read_fn = 'read_array'
		else:
			read_fn = f'read_{type.klass}'
		default_expr = f'std::make_optional({type.default_cpp})' if type.default_cpp != None else 'std::nullopt'
		code = '\n'.join([
			f'params.{name} = {read_fn}(params_json, "{name}", {default_expr}, error);',
			f'if (!error.empty())',
			'\tbreak;',
		])
		# TODO: create actual enum type
		if type.enumerated_values:
			valid_expr = ' || '.join([f'params.{name} == "{v}"' for v in type.enumerated_values])
			code += '\n'.join([
				'',
				f'if (!({valid_expr}))',
				'{',
				f'\terror = fmt::format("invalid value for enumeration: {{}}", params.{name});',
				'\tbreak;',
				'}',
			])
		return textwrap.indent(code, '\t'*indent)

	lines = []
	indent = 0
	add(lines, indent, 'JSON protocol_handle_command(protocol::commands::type type, JSON& params_json)')
	add(lines, indent, '{')
	indent += 1

	add(lines, indent, 'std::string error;')
	add(lines, indent, 'switch (type)')
	add(lines, indent, '{')
	indent += 1

	for descriptor in descriptors:
		name = descriptor.method
		has_params = descriptor.params != None
		has_result = descriptor.result != None

		add(lines, indent, f'case protocol::commands::type::{name}:')
		add(lines, indent, '{')
		indent += 1

		params_expr = '{}'
		if has_params:
			params_expr = 'params'
			add(lines, indent, f'protocol::commands::{name}::params params;')
			for arg, type in descriptor.params.items():
				add(lines, 0, generate_read_json_type_call(indent, arg, type))
		
		handle_expr = f'protocol::commands::{name}::handle({params_expr})'
		if has_result:
			add(lines, indent, f'auto result = {handle_expr};')
			add(lines, indent, 'JSON result_json;')
			for arg, type in descriptor.result.items():
				if type.is_custom() or type.array:
					add(lines, indent, f'write_type(result_json["{arg}"], result.{arg});')
				else:
					add(lines, indent, f'result_json["{arg}"] = result.{arg};')
			add(lines, indent, 'return result_json;')
		else:
			add(lines, indent, f'{handle_expr};')
			add(lines, indent, 'return {};')

		indent -= 1
		add(lines, indent, '}')

	indent -= 1
	add(lines, indent, '}')

	add(lines, indent, '')
	add(lines, indent, 'return JSON({"error", error});')

	indent -= 1
	add(lines, indent, '}')
	add(lines, indent, '')

	return '\n'.join(lines)


def generate_emit_event_fn(event: Event):
	has_params = event.params != None

	lines = []
	indent = 0
	add(lines, indent, f'void protocol::events::{event.method}::emit(protocol::events::{event.method}::params params)')
	add(lines, indent, '{')
	indent += 1

	add(lines, indent, 'JSON params_json;')
	if has_params:
		for arg, type in event.params.items():
			if type.is_custom() or type.array:
				add(lines, indent, f'write_type(params_json["{arg}"], params.{arg});')
			else:
				add(lines, indent, f'params_json["{arg}"] = params.{arg};')

	add(lines, indent, f'protocol_broadcast_event(protocol::events::type::{event.method}, params_json);')

	indent -= 1
	add(lines, indent, '}')
	add(lines, indent, '')

	return '\n'.join(lines)

# Create stubs in `commands.cpp` for newly added commands.
def insert_command_stubs(protocol_config: ProtocolConfig):
	command_names = [c.method for c in protocol_config.commands]

	def get_type_to_line(lines):
		type_to_line = {}
		for type_index, name in enumerate(command_names):
			needle = f'protocol::commands::{name}::handle'
			for i, line in enumerate(lines):
				if needle in line:
					type_to_line[type_index] = i
					break
		return type_to_line

	commands_path = root_dir / 'src/zq/protocol/commands.cpp'
	commands_content = commands_path.read_text('utf-8')

	found_missing = False
	for type_index, name in reversed(list(enumerate(command_names))):
		needle = f'protocol::commands::{name}::handle'
		if needle in commands_content:
			continue

		found_missing = True
		lines = commands_content.splitlines(keepends=True)
		type_to_line = get_type_to_line(lines)
		next_command_index = type_index + 1
		while next_command_index not in type_to_line and next_command_index < len(command_names):
			next_command_index += 1

		stub = f"""
protocol::commands::{name}::result protocol::commands::{name}::handle(protocol::commands::{name}::params params)
{{
	// TODO implement
	protocol::commands::{name}::result result;
	abort();
	return result;
}}
"""
		if next_command_index in type_to_line:
			insert_index = type_to_line[next_command_index] - 1
			lines.insert(insert_index, stub)
		else:
			lines.append(stub)
		commands_content = ''.join(lines)

	if found_missing:
		commands_path.write_text(commands_content, 'utf-8')

def run():
	protocol_config = parse_protocol_config(protocol_config_yml)

	content = f"""// This file is generated by scripts/generate_protocol.py

#ifndef _ZQ_PROTOCOL_IMPL_H_
#define _ZQ_PROTOCOL_IMPL_H_

#include <string>
#include <vector>

{generate_types_header_section(protocol_config.types)}

{generate_header_section('commands', protocol_config.commands)}

{generate_header_section('events', protocol_config.events)}

#endif
"""
	(root_dir / 'src/zq/protocol/impl.h').write_text(content)

	content = '\n'.join([
		'// This file is generated by scripts/generate_protocol.py\n',
		'#include "zq/protocol/impl.h"',
		'#include "zq/protocol/protocol.h"',
		'#include "json/json.h"',
		'#include "fmt/format.h"',
		'#include <optional>',
		'',
		'using giri::json::JSON;',
		'',
		generate_type_parser('command', protocol_config.commands),
		generate_type_to_string('event', protocol_config.events),

		generate_read_json_type_fn('int'),
		generate_read_json_type_fn('string'),
		generate_read_json_type_fn('bool'),

		*[generate_write_json_type_fn(t) for t in builtin_types],
		*[generate_write_json_type_fn(t) for t in custom_types],

		generate_write_json_array_type_fn(),
		generate_handle_command_fn(protocol_config.commands),
		*map(generate_emit_event_fn, protocol_config.events),
	])
	(root_dir / 'src/zq/protocol/impl_gen.cpp').write_text(content)

	insert_command_stubs(protocol_config)

run()
