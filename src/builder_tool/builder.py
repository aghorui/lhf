#!/usr/bin/python

from enum import Enum
import os
import tomllib as toml
import sys
import shutil
import logging
import argparse
import tempfile
import subprocess
import json
from jsonschema import ValidationError, validate
from pathlib import Path
from typing import Dict, Generic, List, Optional, Set, Tuple, TypeVar
from jinja2 import FileSystemLoader, Environment, Template, StrictUndefined

# CONSTANTS #

BUILDER_VERSION = "0.1.0"
LHF_SUPPORTED_VERSION = "0.2.0"

INPUT_GRAMMAR_SCHEMA_PATH: str = "schema.json"
TEMPLATE_PATH: str = 'templates'
DEFAULT_CPP_COMPILERS: List[str] = [ 'gcc', 'clang' ]
CLANG_FORMAT_COMMAND: str = 'clang-format'
CLANG_FORMAT_DEFAULT_STYLE: str = 'LLVM'
DEFAULT_OUTPUT_FILE_NAME: str = "output.hpp"
LHF_CLASS_NAME: str = 'LatticeHashForest'
STATE_STRUCT_NAME: str = 'LHFState'
STORE_STRUCT_NAME: str = 'LHFStore'
LHF_NAMESPACE: str = 'lhf'
LHF_HEADER_FILE: str = 'lhf/lhf.hpp'

DEFAULT_LESS_FUNCTOR: str = f"{LHF_NAMESPACE}::DefaultLess"
DEFAULT_EQUAL_FUNCTOR: str = f"{LHF_NAMESPACE}::DefaultEqual"
DEFAULT_HASH_FUNCTOR: str = f"{LHF_NAMESPACE}::DefaultHash"
DEFAULT_PRINT_FUNCTOR: str = f"{LHF_NAMESPACE}::DefaultPrinter"

# DEFINITIONS #

default_less = lambda t : f"{DEFAULT_LESS_FUNCTOR}<{t}>"
default_equal = lambda t : f"{DEFAULT_EQUAL_FUNCTOR}<{t}>"
default_hash = lambda t : f"{DEFAULT_HASH_FUNCTOR}<{t}>"
default_print = lambda t : f"{DEFAULT_PRINT_FUNCTOR}<{t}>"

BlueprintNodeID = int
ScalarDefinitionID = int

class BlueprintNodeType(Enum):
	INVALID = "INVALID"
	SCALAR = "scalar"
	SET = "set"
	MAP = "map"
	TUPLE = "tuple"

class NestingType(Enum):
	INVALID = "INVALID"
	SCALAR = "NestingScalar" # unused
	IMPLICIT = "IMPLICIT"
	NONE = "NestingNone"
	BASE = "NestingBase"

class TemplateError(Exception):
	def __init__(self, *args: object) -> None:
		super().__init__(*args)

class BlueprintError(Exception):
	def __init__(self, *args: object) -> None:
		super().__init__(*args)

class ScalarDefinition:
	"""
	Defines a value marked as scalar in LHF
	"""
	def __init__(self) -> None:
		self.name: str = ""
		self.less: str | None =  None
		self.equal: str | None = None
		self.hash: str | None =  None
		self.print: str | None = None

	def __eq__(self, value, /) -> bool:
		return isinstance(value, ScalarDefinition) and \
			   self.name == value.name

	def __hash__(self) -> int:
		return hash(self.name)

	def __repr__(self) -> str:
		return f"<ScalarDefinition {self.name}>"


class EntityOperation:
	"""
	Defines an operation within an LHF entity
	"""
	def __init__(self) -> None:
		self.name: str = ""
		self.recursive: bool = False

	def __repr__(self) -> str:
		return f"<oper {self.name} {'rec' if self.recursive else '' }>"

# TODO
# Supposed to automatically add definitions for accessors in for you
# class Accessor:
# 	def __init__(self, accessor_name: str, get_key: bool = False, get_value_index = -1) -> None:
# 		self.accessor_prefix: str = 'get_'
# 		self.accessor_name: str = accessor_name
# 		self.get_key: bool = get_key
# 		self.get_value_index: int = get_value_index

class EntityDefinition:
	"""
	Defines an LHF entity
	"""
	def __init__(self) -> None:
		self.name: str = ""
		self.user_exposed: bool = False
		self.interface: str = ""
		self.operations: List[EntityOperation] = []

	def __repr__(self) -> str:
		return f"<entity {self.name}>"

class BlueprintNode:
	def __init__(self) -> None:
		self.TYPE: BlueprintNodeType = BlueprintNodeType.INVALID
		self.parents: Set[BlueprintNodeID] = set()
		self.use_as: str | None = None

	def __eq__(self, value: object, /) -> bool:
		if isinstance(value, BlueprintNode):
			return self.TYPE == value.TYPE
		else:
			return False

	def parents_str(self):
		if len(self.parents) > 0:
			return str(list(self.parents))
		else:
			return "<root>"

class ScalarNode(BlueprintNode):
	def __init__(self) -> None:
		super().__init__()
		self.TYPE: BlueprintNodeType = BlueprintNodeType.SCALAR
		self.value: ScalarDefinitionID = -1
		self.var_name: str | None = None

	def __eq__(self, value: object, /) -> bool:
		return \
			isinstance(value, ScalarNode) and \
			self.value == value.value

	def __hash__(self) -> int:
		return hash(self.value)

	def __repr__(self) -> str:
		return f"<ScalarNode {self.use_as or ""} s_{self.value} par: {self.parents_str()}>"

class SetNode(BlueprintNode):
	def __init__(self) -> None:
		super().__init__()
		self.TYPE: BlueprintNodeType = BlueprintNodeType.SET
		self.value: BlueprintNodeID = -1

	def __eq__(self, value: object, /) -> bool:
		return \
			isinstance(value, SetNode) and \
			self.value == value.value

	def __hash__(self) -> int:
		return hash((self.TYPE, self.value))

	def __repr__(self) -> str:
		return f"<SetNode {self.use_as or ""} {self.value} par: {self.parents_str()}>"

class MapNode(BlueprintNode):
	def __init__(self) -> None:
		super().__init__()
		self.TYPE: BlueprintNodeType = BlueprintNodeType.MAP
		self.key: ScalarDefinitionID = -1
		self.value: BlueprintNodeID = -1

	def __eq__(self, value: object, /) -> bool:
		return \
			isinstance(value, MapNode) and \
			self.key == value.key and \
			self.value == value.value

	def __hash__(self) -> int:
		return hash((self.TYPE, self.key, self.value))

	def __repr__(self) -> str:
		return f"<MapNode {self.use_as or ""} s_{self.key} {self.value} par: {self.parents_str()}>"

class TupleNode(BlueprintNode):
	def __init__(self) -> None:
		super().__init__()
		self.TYPE: BlueprintNodeType = BlueprintNodeType.TUPLE
		self.value: List[BlueprintNodeID] = []

	def __eq__(self, value: object, /) -> bool:
		return \
			isinstance(value, TupleNode) and \
			len(self.value) == len(value.value) and \
			all([ self.value[i] == value.value[i] for i in range(len(self.value)) ])

	def __hash__(self) -> int:
		return hash((self.TYPE, *self.value))

	def __repr__(self) -> str:
		return f"<TupleNode {self.use_as or ""} {self.value} par: {self.parents_str()}>"

T = TypeVar('T')

class Deduplicator(Generic[T]):
	"""
	The Deduplicator deduplicates values based on an equality criteria.
	A unique mapping is generated with the value (or it's python id()) as a
	key to an index in a storage array.
	"""
	def __init__(self, hash_ids = False) -> None:
		self.storage_array: List[T] = []
		self.storage_map: Dict[T | int, int] = {}
		self.hash_ids: bool = hash_ids

	def insert(self, value: T) -> int:
		if value not in self.storage_map:
			self.storage_array.append(value)
			if self.hash_ids:
				self.storage_map[id(value)] = len(self.storage_array) - 1
			else:
				self.storage_map[value] = len(self.storage_array) - 1
		return self.storage_map[value]

	def get_index(self, value: T):
		if value not in self.storage_map:
			raise KeyError(f"Value {value} not found in storage")
		if self.hash_ids:
			return self.storage_map[id(value)]
		else:
			return self.storage_map[value]

	def get_object_from_index(self, index: int):
		if (index > len(self.storage_array) - 1) or (index < 0):
			raise KeyError(f"Supplied index ({index}) is invalid (max: {len(self.storage_array)})")
		return self.storage_array[index]

	def __contains__(self, value: T) -> bool:
		if (self.hash_ids):
			return id(value) in self.storage_map
		else:
			return value in self.storage_map

	def __iter__(self):
		return iter(self.storage_array)

class Config:
	"""
	Configuration variables for the program
	"""
	def __init__(self) -> None:
		self.blueprintfile: Path = Path()
		self.no_static: bool = False
		self.include_files: List[Path] = []
		self.compile_command: str = ""
		self.include_dirs: List[Path] = []
		self.output_path: Path = Path(os.getcwd()) / DEFAULT_OUTPUT_FILE_NAME
		self.lhf_header: str = LHF_HEADER_FILE
		self.format_style: str = CLANG_FORMAT_DEFAULT_STYLE
		self.ignore_checks: bool = False
		self.generate_graph: bool = False
		self.generate_graph_path: Path = Path()

		self.lhf_version: str = ''
		self.namespace: str = ''
		self.scalar_definitions: List[ScalarDefinition] = []
		self.scalar_definition_ids: Dict[str, int] = {}
		self.entity_definitions: Dict[str, EntityDefinition] = {}
		self.blueprint: List[dict] = []

	def __repr__(self) -> str:
		return \
			 "Config {\n" + \
			f"    Blp. file path: {self.blueprintfile}\n" \
			f"    Static: {self.no_static}\n" \
			f"    Include files: {self.include_files}\n" \
			f"    Include dirs: {self.include_files}\n" \
			f"    LHF header file path: {self.lhf_header}\n" \
			f"    Ignore checks: {self.ignore_checks}\n" \
			f"    Generate Graph: {'No' if not self.generate_graph else self.generate_graph_path}\n" \
			f"    ---\n" \
			f"    LHF Version Required: '{self.lhf_version}'\n" \
			f"    Namespace: '{self.namespace}'\n" \
			f"    Scalar Definitions: {self.scalar_definitions}\n" \
			f"    Entity Definitions: {self.entity_definitions}\n" \
			f"    Blueprint: { f'[size:{len(self.blueprint)}]' \
				  if self.blueprint != None else '<none>' }\n" \
			"}\n"

# GLOBALS #

logger: logging.Logger = logging.getLogger("Builder")

# FUNCTIONS #

def code_formatter_exists() -> bool:
	return shutil.which(CLANG_FORMAT_COMMAND) != None

def format_code(code: str, style: str = "LLVM") -> str:
	"""
	Formats the code using `clang-format`

	:param      code:   The code
	:type       code:   str
	:param      style:  clang-format style string
	:type       style:  str

	:returns:   Formatted code
	:rtype:     str
	"""
	result = subprocess.run(
		[CLANG_FORMAT_COMMAND, f"--style={style}"],
		input=code.encode(),
		stdout=subprocess.PIPE,
		stderr=subprocess.PIPE
	)

	if result.returncode != 0:
		raise RuntimeError(
			f"Could not run ${CLANG_FORMAT_COMMAND}: "
			f"'${result.stdout.decode()}'")
	return result.stdout.decode()

def compile_file(
	compiler: str,
	input_path: Path,
	output_path: Path,
	flags: List[str] = []) -> bool:
	"""
	Tries to compile a file and returns whether it was successful or not.
	:param      compiler:     The compiler invocation string
	:type       compiler:     str
	:param      input_path:   The input path
	:type       input_path:   Path
	:param      output_path:  The output path
	:type       output_path:  Path
	:param      flags:        Additional compiler flags
	:type       flags:        List[str]
	:returns:   True on success. False otherwise.
	:rtype:     bool
	"""
	result = subprocess.run(
		[ compiler, input_path, '-c', '-o', output_path, *flags ],
		stdout=subprocess.PIPE,
		stderr=subprocess.STDOUT,
		text=True
	)

	if result.returncode != 0:
		logger.error("Could not compile file")
		logger.info("Printing compiler output:\n" + result.stdout)
		return False
	else:
		logger.info("Compilation successful")
		return True

def initialize_config() -> Config:
	"""
	Initializes the Config structure and all user input from the command
	line arguments and the Blueprint file.

	:returns:   The resultant Config object
	:rtype:     Config
	"""
	logging.basicConfig(
		level=logging.INFO,
		format='[%(levelname)s] %(message)s')

	parser = argparse.ArgumentParser(
		description="Convert an LHF blueprint description to code. WARNING: "
		            "This tool is experiemntal.",
		epilog="clang-format must be installed in your system if you want "
		       "the generated output to be fomatted automatically for you.")

	parser.add_argument(
		'bfile',
		help="Path to the configuration/blueprint file")

	parser.add_argument(
		'--no-static', '-ns',
		action='store_true',
		help="Do not generate static global LHFs. Thread a state object instead.")

	parser.add_argument(
		'--include-file', '-i',
		action='append',
		help="File path to include for code generation (can be specified multiple times)")

	parser.add_argument(
		'--include-dir', '-I',
		action='append',
		help="Include directories to be passed to the compiler (can be spcified multiple times)")

	parser.add_argument(
		'--output-path', '-o',
		help="Code generation output path (default is \'.\')")

	parser.add_argument(
		'--lhf-header',
		help="Relative/absolute path override to the LHF header file")

	parser.add_argument(
		'--format-style',
		help="Specifies the code formatting style for the generated output"
		     " (clang-format is used as the formatter if present)")

	parser.add_argument(
		'--compile-command', '-cc',
		help="Specifies the command to compile source files with, overriding the defaults.")

	parser.add_argument(
		'--ignore-checks',
		action='store_true',
		help="Ignore errors from checks on processing and generation")

	parser.add_argument(
		'--generate-graph',
		metavar="PATH",
		help="Generate a graphviz-based dot-file of the blueprint to the specified location")

	args = parser.parse_args()
	config = Config()

	if not args.bfile:
		logger.error("No blueprint file specified.")
		sys.exit(1)

	config.blueprintfile = Path(args.bfile)

	if args.include_file:
		config.include_files = [ Path(p) for p in args.include_file ]

	if args.include_dir:
		config.include_dirs = [ Path(p) for p in args.include_dir ]

	if args.output_path:
		config.output_path = Path(args.output_path)

	if args.lhf_header:
		config.lhf_header = args.lhf_header

	if args.format_style:
		config.format_style = args.format_style

	if args.compile_command:
		config.compile_command = args.compile_command

	if args.ignore_checks:
		config.ignore_checks = args.ignore_checks

	if args.generate_graph:
		config.generate_graph = True
		config.generate_graph_path = Path(args.generate_graph)

	if not config.blueprintfile.exists():
		logger.error(f"Blueprint file {config.blueprintfile} does not exist")
		sys.exit(1)

	try:
		with open(config.blueprintfile, 'r') as blp, \
			 open(INPUT_GRAMMAR_SCHEMA_PATH, 'r') as grammar:

			if config.blueprintfile.suffix[1:] == "json":
				blp_json = json.loads(blp.read())
			elif config.blueprintfile.suffix[1:] == "toml":
				blp_json = toml.loads(blp.read())
			else:
				raise BlueprintError("Blueprint filename should end with either 'json' or 'toml'")

			grammar_json = json.loads(grammar.read())
			validate(blp_json, grammar_json)

			config.lhf_version = blp_json['lhf_version']
			config.namespace = blp_json['namespace']

			scalar_definition_autonumber: int = 0

			for key in blp_json['scalar_definitions']:
				entry = blp_json['scalar_definitions'][key]

				s = ScalarDefinition()

				if key in config.scalar_definitions:
					raise BlueprintError(f"Duplicate entry exists for scalar type: {entry['name']}")

				s.name = entry['name']

				if 'less' in entry:
					s.less = entry['less']
				else:
					s.less = default_less(s.name)

				if 'equal' in entry:
					s.equal = entry['equal']
				else:
					s.equal = default_equal(s.name)

				if 'hash' in entry:
					s.hash = entry['hash']
				else:
					s.hash = default_hash(s.name)

				if 'print' in entry:
					s.print = entry['print']
				else:
					s.print = default_print(s.name)

				config.scalar_definitions.append(s)
				config.scalar_definition_ids[key] = scalar_definition_autonumber
				scalar_definition_autonumber += 1

			for key in blp_json['entity_definitions']:
				entry = blp_json['entity_definitions'][key]
				e = EntityDefinition()

				if key in config.entity_definitions:
					raise BlueprintError(f"Duplicate entry exists for entity definition: {entry['name']}")

				e.name = entry['name']
				e.interface = entry['interface']
				e.user_exposed = entry['user_exposed']
				operation_names = set()
				for operation in entry['operations']:
					if operation['name'] in operation_names:
						raise BlueprintError(f"Duplicate entry exists for operation: {operation['name']}")
					op = EntityOperation()
					op.name = operation['name']
					op.recursive = operation['recursive']
					e.operations.append(op)
					operation_names.add(op.name)

				config.entity_definitions[key] = e

			config.blueprint = blp_json['blueprint']

	except json.JSONDecodeError as e:
		logger.error(f"Could not decode JSON: {e}")
		sys.exit(1)
	except toml.TOMLDecodeError as e:
		logger.error(f"Could not decode TOML: {e}")
		sys.exit(1)
	except BlueprintError as e:
		logger.error(f"Blueprint error: {e}")
	except ValidationError as e:
		logger.error(f"Could not validate JSON: {e.message} (at {e.json_path})")
		sys.exit(1)
	except FileNotFoundError as e:
		logger.error(f"Could not load file: {e}")
		sys.exit(1)

	return config

def template_exception(s: str):
	raise TemplateError(s)

def initialize_templates() -> Environment:
	"""
	Initializes the templates.
	:returns:   The Jinja2 Environment for templates.
	:rtype:     Environment
	"""
	env: Environment = Environment(
		loader = FileSystemLoader(TEMPLATE_PATH),
		undefined=StrictUndefined)
	env.globals['NestingType'] = NestingType
	env.globals['raise'] = template_exception
	return env

def create_object_from_node(
	node: dict,
	node_id_map: Dict[int, int],
	scalar_name_id_map: Dict[str, int],
	node_dedup: Deduplicator[BlueprintNode]) -> BlueprintNode:
	"""
	Creates an BlueprintNode from a JSON node.

	:param      node:                The JSON Node object
	:type       node:                dict
	:param      node_id_map:         Mapping from the unique identifier of JSON nodes
	:type       node_id_map:         Dict[int, int]
	:param      scalar_name_id_map:  Map of scalar identifiers to their indices
	                                 in the scalar definition storage
	:type       scalar_name_id_map:  Dict[str, int]
	:param      node_dedup:          Deduplicating storage for Blueprint nodes
	:type       node_dedup:          Deduplicator[BlueprintNode]

	:returns:   The resultant Blueprint node
	:rtype:     BlueprintNode
	"""
	node_object = None

	if node['type'] == BlueprintNodeType.SCALAR.value:
		node_object = ScalarNode()
		if (node['value'] not in scalar_name_id_map):
			raise BlueprintError(f"No definition for scalar type: {node['value']}")
		if 'var_name' in node:
			node_object.var_name = node['var_name']
		node_object.value = scalar_name_id_map[node['value']]

	elif node['type'] == BlueprintNodeType.SET.value:
		node_object = SetNode()
		if (id(node['value']) not in node_id_map):
			raise BlueprintError(f"Internal error. Node not registered: {node['value']}")
		node_object.value = node_id_map[id(node['value'])]

	elif node['type'] == BlueprintNodeType.MAP.value:
		node_object = MapNode()
		if (id(node['value']) not in node_id_map):
			raise BlueprintError(f"Internal error. Node not registered: {node['value']}")
		node_object.value = node_id_map[id(node['value'])]

		if (node['key'] not in scalar_name_id_map):
			raise BlueprintError(f"No definition for scalar type: {node['key']}")
		node_object.key = scalar_name_id_map[node['key']]

	elif node['type'] == BlueprintNodeType.TUPLE.value:
		node_object = TupleNode()
		for i in node['value']:
			if (id(node['value']) not in node_id_map):
				raise BlueprintError(f"Internal error. Node not registered: {i}")
			node_object.value.append(node_id_map[id(node['value'])])
	else:
		raise BlueprintError(f"Unknown node type: {node['type']}")

	if 'use_as' in node:
		node_object.use_as = node['use_as']

	return node_object

def process_node(node: dict) -> List:
	"""
	Returns a list of children based on the JSON Blueprint node type.

	:param      node:  The JSON Blueprint node
	:type       node:  dict

	:returns:   List of child JSON Blueprint nodes.
	:rtype:     List
	"""

	children: List = []
	node_object = None

	if node['type'] == BlueprintNodeType.SCALAR.value:
		pass

	elif node['type'] == BlueprintNodeType.SET.value:
		children.append(node)
		children.append(node['value'])

	elif node['type'] == BlueprintNodeType.MAP.value:
		children.append(node)
		children.append(node['value'])

	elif node['type'] == BlueprintNodeType.TUPLE.value:
		children.append(node)
		for i in reversed(node['value']):
			children.append(i)

	return children

class LHFClassDefinition:
	def __init__(self,
		is_tuple: bool,
		entity_valid: bool,
		var_name: str,
		class_name: str,
		key_definition: ScalarDefinition | Optional['LHFClassDefinition'],
		nesting_type: NestingType,
		nesting_elems: List[str],
		dependencies: List[str]) -> None:
		self.is_tuple: bool = is_tuple
		self.entity_valid: bool = entity_valid
		self.var_name: str = var_name
		self.class_name: str = class_name
		self.key_definition: ScalarDefinition | Optional['LHFClassDefinition'] = key_definition
		self.nesting_type: NestingType = nesting_type
		self.nesting_elems: List[str] = nesting_elems
		self.dependencies: List[str] = dependencies

	def __repr__(self) -> str:
		return "{\n" \
		f"    is_tuple = {self.is_tuple}\n" \
		f"    entity_valid = {self.entity_valid}\n" \
		f"    var_name = {self.var_name}\n" \
		f"    class_name = {self.class_name}\n" \
		f"    key_definition = {self.key_definition}\n" \
		f"    nesting_type = {self.nesting_type}\n" \
		f"    nesting_elems = {self.nesting_elems}\n" \
		f"    dependencies = {self.dependencies}\n" \
		"}"


# VALID:
# Valid: Set -> Scalar              NO Nesting
# Valid: Set -> Set                 IMPLICIT Nesting
# Valid: Map(Scalar) -> Set         EXPLICIT Nesting
# Valid: Map(Scalar) -> Tuple(Set)  EXPLICIT Nesting
# Valid: Map(Scalar) -> Map(Scalar) EXPLICIT Nesting
# Valid: Set -> Tuple               IMPLICIT Nesting
#
# INVALID (NOT EXHAUSTIVE):
# Invalid: Map(Scalar) -> Scalar     NO Nesting <Not handled>
def get_names_from_node(
	config: Config,
	node_dedup: Deduplicator[BlueprintNode],
	blp_id: BlueprintNodeID,
	existing_defs: Dict[BlueprintNodeID, LHFClassDefinition]) -> LHFClassDefinition:
	if blp_id in existing_defs:
		raise ValueError(f"Blueprint node {blp_id} already present in existing definitions")

	blp = node_dedup.get_object_from_index(blp_id)

	if isinstance(blp, ScalarNode):
		s: ScalarDefinition = config.scalar_definitions[blp.value]
		return LHFClassDefinition(
			is_tuple=False,
			entity_valid=False,
			class_name="<INVALID: SCALAR>",
			var_name="<INVALID: SCALAR>",
			key_definition=s,
			nesting_type=NestingType.INVALID,
			nesting_elems=[],
			dependencies=[]
		)

	if isinstance(blp, SetNode):
		assert(blp.use_as != None)
		if blp.value not in existing_defs:
			raise ValueError(f"Blueprint node {blp.value} expected to be "
			                 f"present in processed nodes but not yet present.")

		if existing_defs[blp.value].entity_valid:
			# print("IMPLICIT NESTING")
			return LHFClassDefinition(
				is_tuple=False,
				entity_valid=True,
				class_name=config.entity_definitions[blp.use_as].name,
				var_name=blp.use_as,
				key_definition=existing_defs[blp.value],
				nesting_type=NestingType.IMPLICIT,
				nesting_elems=[],
				dependencies=[]
			)
		else:
			# print("NO NESTING")
			return LHFClassDefinition(
				is_tuple=False,
				entity_valid=True,
				class_name=config.entity_definitions[blp.use_as].name,
				var_name=blp.use_as,
				key_definition=existing_defs[blp.value].key_definition,
				nesting_type=NestingType.NONE,
				nesting_elems=[],
				dependencies=[]
			)

	elif isinstance(blp, MapNode):
		assert(blp.use_as != None)
		if blp.key not in existing_defs:
			raise ValueError(f"Blueprint node {blp.key} expected to be "
			                 f" present in processed nodes but not yet present.")

		if blp.value not in existing_defs:
			raise ValueError(f"Blueprint node {blp.value} expected to be "
			                 f"present in processed nodes but not yet present.")

		value_def = existing_defs[blp.value]
		nesting_elems = None
		dependencies = None

		if not value_def.is_tuple:
			nesting_elems = [ value_def.class_name ]
			dependencies = [ value_def.var_name ]
		else:
			nesting_elems = [ *value_def.nesting_elems ]
			dependencies = [ *value_def.dependencies ]

		return LHFClassDefinition(
			is_tuple=False,
			entity_valid=True,
			class_name=config.entity_definitions[blp.use_as].name,
			var_name=blp.use_as,
			key_definition=existing_defs[blp.key].key_definition,
			nesting_type=NestingType.BASE,
			nesting_elems=nesting_elems,
			dependencies=dependencies
		)

	elif isinstance(blp, TupleNode):
		for v in blp.value:
			if v not in existing_defs:
				raise ValueError(f"Blueprint node {v} expected to be "
			                     f"present in processed nodes but not yet present.")

		return LHFClassDefinition(
			is_tuple=True,
			entity_valid=False,
			class_name="<INVALID: TUPLE>",
			var_name="<INVALID: TUPLE>",
			key_definition=None,
			nesting_type=NestingType.BASE,
			nesting_elems=[ existing_defs[k].class_name for k in blp.value ],
			dependencies=[ existing_defs[k].var_name for k in blp.value ]
		)
	else:
		raise ValueError(f"Unknown Node Type: {type(blp)}")

def generate_output(
	tenv: Environment,
	config: Config,
	node_dedup: Deduplicator[BlueprintNode],
	entity_mappings: Dict[str, BlueprintNodeID],
	canonical_mappings: Dict[BlueprintNodeID, str],
	ordering: List[BlueprintNodeID]) -> Path:
	"""
	Generates the actual output header file

	:param      tenv:                Jinja template environment
	:type       tenv:                Environment
	:param      config:              The configuration
	:type       config:              Config
	:param      node_dedup:          Blueprint node storage
	:type       node_dedup:          Deduplicator[BlueprintNode]
	:param      entity_mappings:     The entity mappings
	:type       entity_mappings:     Dict[str, BlueprintNodeID]
	:param      canonical_mappings:  The canonical entity mappings
	:type       canonical_mappings:  Dict[BlueprintNodeID, str]
	:param      ordering:            Topological ordering of blp. nodes
	:type       ordering:            List[BlueprintNodeID]

	:returns:   Path to the output file
	:rtype:     Path
	"""

	t: Template

	if config.no_static:
		t = tenv.get_template("lhf_definition.jinja2.cpp")
	else:
		t = tenv.get_template("lhf_definition_static.jinja2.cpp")

	visited_nodes = set()

	entities = []
	entity_aliases = []
	interface_aliases = []

	ldefs: Dict[BlueprintNodeID, LHFClassDefinition] = {}

	for blp_id in ordering:
		ldef = get_names_from_node(config, node_dedup, blp_id, ldefs)
		ldefs[blp_id] = ldef

	for key in entity_mappings:
		if entity_mappings[key] in visited_nodes:
			canonical_entity = canonical_mappings[entity_mappings[key]]
			entity_aliases.append({
				"from": config.entity_definitions[key].name,
				"to": config.entity_definitions[canonical_entity].name
			})
			interface_aliases.append({
				"from": config.entity_definitions[key].interface,
				"to": config.entity_definitions[canonical_entity].interface })
			continue;
		else:
			visited_nodes.add(entity_mappings[key])

		ldef = ldefs[entity_mappings[key]]
		assert(ldef.key_definition != None)

		if ldef.nesting_type == NestingType.IMPLICIT:
			assert(isinstance(ldef.key_definition, LHFClassDefinition))
			type_name: str = f"{ldef.key_definition}::Index";
			type_name_hash: str = f"{ldef.key_definition}::Index::Hash";
			entities.append({
				"name": config.entity_definitions[key].name,
				"var_name": key,
				"type_name": type_name,
				"less": default_less(type_name),
				"hash": type_name_hash,
				"equal": default_equal(type_name),
				"print": default_print(type_name),
				"nesting_type": ldef.nesting_type,
				"nesting_elems": ldef.nesting_elems,
				"dependencies": ldef.dependencies,
				"interface_name": config.entity_definitions[key].interface
			})
		else:
			assert(isinstance(ldef.key_definition, ScalarDefinition))
			entities.append({
				"name": config.entity_definitions[key].name,
				"var_name": key,
				"type_name": ldef.key_definition.name,
				"less": ldef.key_definition.less,
				"hash": ldef.key_definition.hash,
				"equal": ldef.key_definition.equal,
				"print": ldef.key_definition.print,
				"nesting_type": ldef.nesting_type,
				"nesting_elems": ldef.nesting_elems,
				"dependencies": ldef.dependencies,
				"interface_name": config.entity_definitions[key].interface
			})

	s: str = t.render({
		"lhf_name": LHF_CLASS_NAME,
		"lhf_version": config.lhf_version,
		"blp_version": BUILDER_VERSION,
		"lhf_namespace": LHF_NAMESPACE,
		"state_struct_name": STATE_STRUCT_NAME,
		"store_struct_name": STORE_STRUCT_NAME,
		"include_files": config.include_files,
		"lhf_header": config.lhf_header,
		"namespace_value": config.namespace,
		"entities": entities,
		"entity_aliases": entity_aliases,
		"interface_aliases": interface_aliases
	}, undefined=StrictUndefined)

	s = format_code(s, style=config.format_style)

	output_file_path: Path = config.output_path
	with open(config.output_path, 'w') as f:
		f.write(s)

	return output_file_path

def generate_graph(config: Config, node_dedup: Deduplicator[BlueprintNode]) -> str:
	ret: str = ""
	roots = []
	ret += "digraph Blueprint {\n"
	ret += "	node [shape=box];\n"
	for i in range(len(node_dedup.storage_array)):
		node = node_dedup.storage_array[i]
		from_str = str(node_dedup.storage_array[i])

		if len(node.parents) == 0:
			roots.append(node)

		if isinstance(node, ScalarNode):
			to_str = config.scalar_definitions[node.value]
			ret += f"	\"{from_str}\" -> \"{to_str}\";\n"
		elif isinstance(node, SetNode):
			to_str = node_dedup.storage_array[node.value]
			ret += f"	\"{from_str}\" -> \"{to_str}\";\n"
		elif isinstance(node, MapNode):
			to_key_str = config.scalar_definitions[node.key]
			to_str = node_dedup.storage_array[node.value]
			ret += f"	\"{from_str}\" -> \"{to_key_str}\";\n"
			ret += f"	\"{from_str}\" -> \"{to_str}\";\n"
		elif isinstance(node, TupleNode):
			for i in node.value:
				to_str = config.scalar_definitions[i]
				ret += f"	\"{from_str}\" -> \"{to_str}\";\n"

	ret += "	subgraph {\n"
	ret += "		rank = same;\n"
	for node in roots:
		from_str = str(node)
		ret += f"		\"{from_str}\" [style=filled, fillcolor=gray];\n"
	ret += "	}\n"
	ret += "}\n"

	return ret

def process_blueprint(config: Config) -> \
	Tuple[
		Deduplicator[BlueprintNode],
		Dict[str, BlueprintNodeID],
		Dict[BlueprintNodeID, str],
		List[BlueprintNodeID]]:
	"""
	Processes the Blueprint information and converts them into nodes.
	deduplication of unique nodes is performed in this step.

	The algorithm performs a postorder traversal on each supplied subtree such
	that a topological ordering is created. Scalar nodes are consequently
	resolved first, and then incrementally, based on the children, the parent
	trees are resolved and deduplicated in a bottom-up fashion.

	Hashing/deduplicating the child nodes first allows us to use unique
	identifiers for the children instead of some form of recursive calculation
	to determine the uniqueness of the parents.

	:param      config:  The configuration
	:type       config:  Config

	:returns:   Deduplicated blp. nodes, entity to blp. node mapping, and a
	            topological ordering or blp. nodes.
	:rtype:     Tuple[Deduplicator[BlueprintNode],
	                  Dict[str, BlueprintNode],
	                  List[BlueprintNode]]
	"""

	node_dedup: Deduplicator[BlueprintNode] = Deduplicator()
	node_id_map: Dict[int, BlueprintNodeID] = {}
	entity_mappings: Dict[str, BlueprintNodeID] = {}
	canonical_mappings: Dict[BlueprintNodeID, str] = {}
	ordering: List[BlueprintNodeID] = []

	for struct in config.blueprint:
		stack: List = []
		current: dict | None = None
		prev: dict | None = None

		stack.append(struct)

		while len(stack) > 0:
			current = stack.pop()
			assert current != None

			# This returns the current node, concatenated with it's children or
			# an empty list.
			# It's done this way to make stack appending easier.
			children = process_node(current)

			if (len(children) == 0) or (prev in children):
				# Register the new node and get IDs
				# Map the JSON dict to a unique index
				obj = create_object_from_node(
					current,
					node_id_map,
					config.scalar_definition_ids,
					node_dedup)

				present = obj in node_dedup
				index = node_dedup.insert(obj)
				node_id_map[id(current)] = index

				# Connect parents of the children
				# We remove the first index because it's the current node.
				for child_json in children[1:]:
					child = node_dedup.get_object_from_index(node_id_map[id(child_json)])
					child.parents.add(index)

				if not present:
					ordering.append(index)

				if obj.use_as != None:
					if obj.use_as not in config.entity_definitions:
						raise BlueprintError(
							f"Entity '{obj.use_as}' doesn't exist.")

					if obj.use_as in entity_mappings:
						raise BlueprintError(
							f"Entity '{obj.use_as}' targeted by multiple blueprint nodes.")

					if obj.TYPE == BlueprintNodeType.SCALAR or \
						obj.TYPE == BlueprintNodeType.TUPLE:
						raise BlueprintError(
							f"Scalars or tuples cannot be used as entities (on processing '{obj.use_as}')")

					if index not in canonical_mappings:
						canonical_mappings[index] = obj.use_as

					entity_mappings[obj.use_as] = index
			else:
				stack.extend(children)

			prev = current

	print("\nSCALAR DEFINITIONS:\n"
		  "===================")
	for i in range(len(config.scalar_definitions)):
		print(f"{i}: {config.scalar_definitions[i]}")

	print("\nBLUEPRINT NODES:\n"
		  "=================")
	for i in range(len(node_dedup.storage_array)):
		print(f"{i}: {node_dedup.storage_array[i]}")

	print("\nENTITY MAPPINGS:\n"
		  "================")
	for i in entity_mappings:
		print(f"{i} -> {entity_mappings[i]}")

	print("\nCANONICAL MAPPINGS:\n"
		  "===================")
	for i in canonical_mappings:
		print(f"{i} -> {canonical_mappings[i]}")
	print("\n")

	if config.generate_graph:
		graph_data = generate_graph(config, node_dedup)
		with open(config.generate_graph_path, 'w') as f:
			f.write(graph_data)
		logger.info(f"Graph written to {config.generate_graph_path}")

	# The ordering could possibly be constructed from the deduplicator,
	# considering it's in insertion order, but we want to keep those concerns
	# seperate to prevent future bugs with two+ levels of indirection.
	return (node_dedup, entity_mappings, canonical_mappings, ordering)

# MAIN #

def main() -> None:
	config: Config = initialize_config()

	env: Environment = initialize_templates()

	# print(config)

	node_dedup, entity_mappings, canonical_mappings, ordering = process_blueprint(config)

	logger.info("Verifying supplied type information")
	t: Template = env.get_template('verify_types.jinja2.cpp')

	eval_types = []

	for d in config.scalar_definitions:
		eval_types.append(d.name)
		if d.less: eval_types.append(d.less)
		if d.hash: eval_types.append(d.hash)
		if d.equal: eval_types.append(d.equal)
		if d.print: eval_types.append(d.print)

	s: str = t.render(
		include_files = config.include_files,
		eval_types = eval_types,
		lhf_header = config.lhf_header,
		undefined=StrictUndefined)

	with tempfile.TemporaryDirectory(prefix='lhfblp_') as tmpdir:
		base_path = Path(tmpdir)
		source_path = base_path / 'def_eval.cpp'
		dest_path = base_path / 'def_eval.o'
		with open(source_path, 'w') as source_file:
			source_file.write(s)

		result: bool = False
		compiler_ran: bool = False;
		compilers: List[str] = []
		flags: List[str] = []

		if config.compile_command != "":
			compilers.append(config.compile_command)
		compilers.extend(DEFAULT_CPP_COMPILERS)

		for dir_path in config.include_dirs:
			flags.append(f"-I{dir_path}")

		for compiler in DEFAULT_CPP_COMPILERS:
			if shutil.which(compiler):
				result = compile_file(compiler, source_path, dest_path, flags)
				compiler_ran = True
				break
			else:
				logger.info(f"Compiler '{compiler}' not found. Skipping.")

		if not compiler_ran:
			logger.error("No compiler available to verify input types.")
			if config.ignore_checks:
				logger.info("Ignoring check as requested.")
			else:
				logger.error("Aborting.")
				sys.exit(2)
		else:
			if not result:
				logger.info("Test File Contents:")
				logger.info("\n" + s + "\n")
				logger.error("Verification failed.")
				if config.ignore_checks:
					logger.info("Ignoring check as requested.")
				else:
					logger.error("Aborting.")
					sys.exit(2)
			else:
				logger.info("Verification Passed")

	output_file_path: Path = generate_output(
		env,
		config,
		node_dedup,
		entity_mappings,
		canonical_mappings,
		ordering)

	logger.info(f"Output written to {output_file_path}")

if __name__ == '__main__':
	main()
	sys.exit(0)