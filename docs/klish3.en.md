# Klish 3


## Introduction

The klish program is designed to organize a command line interface, in
which only a strictly defined set of commands is available to the
operator.  This distinguishes klish from standard shell interpreters,
where the operator can use any commands that exist in the system and the
ability to execute them depends only on access rights.  The set of
commands available in klish is defined by configuration stage and can be
set, in the general case, in various ways, the main one being XML
configuration files.  Commands in klish are not are system commands, but
are fully defined in configuration files, with all possible options,
syntax and actions they perform.

The main application of klish is embedded systems, where the operator
has access to only certain device-specific commands, not an arbitrary
set of commands, as in general-purpose systems. In such embedded
systems, klish can replace the shell interpreter, which is not available
for operator.

An example of embedded systems is managed network equipment.
Historically, this segment has developed two main approaches to
organizing command line interface. Conventionally, they can be called
the "Cisco" approach and the "Cisco" approach.  Juniper. Both Cisco and
Juniper have two modes of operation - command mode and configuration
mode. In command mode, entered commands are immediately are executed,
but do not affect the system configuration. These are the view commands
status, commands to control the device, but not to change its
configuration.  In configuration mode equipment and services are being
configured. In Cisco, configuration commands are also are executed
immediately, changing the system configuration. In Juniper configuration
can be conventionally imagined as a text file, changes in which are made
using standard editing commands. When editing the changes are not
applied to the system. And only by special command the accumulated
complex of changes is applied at once, which ensures consistency of
changes. With the Cisco approach, similar behavior can also be emulate
by projecting commands in a certain way, but for Cisco this is less
natural way.

Which of the approaches is better and easier in a particular case is
determined by the task.  Klish is primarily designed for the Cisco
approach, i.e., for immediate commands.  However, the project has a
system of plugins that allows you to expand it possibilities.  So the
klish-plugin-sysrepo plugin, implemented by a separate project, working
on the basis of the sysrepo repository, allows you to organize the
Juniper approach.


## Basic information

The core of the klish project is the libklish library.  Based on it, the
klish client is built and klishd server.  The main work is done by the
klishd server.  It loads command configuration and waits for requests
from clients.  Interaction between clients and the server takes place
over UNIX sockets using the KTP protocol (*Klish Transfer Protocol*),
developed for this purpose.  Task client - transferring input from the
operator to the server and receiving from it result to be shown to the
operator.  The client does not know what commands exist, how to do them.
All this is done by the server side.  Since the client has relatively
simple code, it is not difficult to implement alternative clients, such
as a graphical client or a client for automated management.  Only the
klish text client has been written so far.  When the client connects to
the server, a separate process is spawned to serve specific client.
When the session ends, the process is also terminated.  Thus, a typical
use of klish is a server pre-launched on the system `klishd` and clients
connecting to it as needed.

Klish has two types of plugins: plugins for loading command
configuration (directory `dbs/` in the source tree), and plugins that
implement actions for commands (directory `plugins/` in the source
tree).

 * `/etc/klish/klishd.conf`: used to configure the server parameters.
   An alternative configuration file can be specified when starting the
   klishd server.
 * `/etc/klish/klish.conf`: configuration file is used to configure
   client settings.  An alternative configuration file can be specified
   when starting the `klish` client.


## Load command configuration

klish includes the following db (data base) plugins for download command
configurations (schemes):

 * expat - Uses the expat library to load configuration from XML.
 * libxml2 - Uses the libxml2 library to load configuration from XML.
 * roxml - Uses the roxml library to load configuration from XML.
 * ischeme - Uses built-in C-code configuration (Internal Scheme).

There is an internal schema representation that is the same as ischeme.
The rest of the plugins translate the external view into ischeme, and klish
handles ischeme internally.

Installed dbs plugins are in `/usr/lib/klish/dbs` (if configured
build with --prefix=/usr). Their names are `kdb-<name>.so`, for example
`/usr/lib/klish/dbs/kdb-libxml2.so`.


## Executable Function Plugins

Each klish command performs some action or several actions at once.
These actions must be described somehow. If you look inside the
implementation, then klish can only run executable compiled code from
the plugin. Plugins contain so-called symbols (symbol, sym), which, in
fact, represent functions with a single fixed API. These symbols can
be referred to klish commands. In turn, the symbol can execute complex
code, for example launch a shell interpreter with the script specified
in the command description klish in the config file. Or another symbol
can execute a Lua script.

Klish can only get symbols from plugins. Standard symbols implemented
in plugins included in the klish project. klish includes the following
plugins:

 * klish - Basic plugin. Navigation, data types (for parameters),
   secondary functions.
 * lua - Execution of Lua scripts. The mechanism is built into the
   plugin and does not use external program for interpretation.
 * script - Execution of scripts. Considers shebang to determine which
   interpreter to use. Thus, not only shell scripts, but also scripts in
   other interpreted languages. Default shell interpreter is used.

Users can write their own plugins and use their own symbols in klish
commands. Installed plugins are in `/usr/lib/klish/plugins` (if you
configure the build with --prefix=/usr). Their names are
`kplugin-<name>.so`, e.g. `/usr/lib/klish/plugins/kplugin-script.so`.

Symbols are either "synchronous" or "asynchronous". Synchronized symbols
are executed in the klishd address space, a separate process is spawned
for asynchronous ones.


## Configuration XML structure

XML files are the main way to describe klish commands today.  In this
section, all examples will be based on XML elements.


### Scopes

Commands are organized into "scopes" called VIEWs.  Those. every team
belongs to some VIEW and is defined in it.  When working in klish always
there is a "current path".  By default, when starting klish with the
current path a VIEW named "main" is assigned.  The current path
determines which commands are currently moment are visible to the
operator.  The current path can be changed [navigation
commands](#navigation).  For example move to the "neighbor" VIEW, then
this neighboring VIEW will become the current path, replacing the old
one. Another possibility is to "go deep", i.e. go to nested VIEW.  Then
the current path will become two-level, similar to how you can go to
nested directory in the file system. When the current path has more than
one level, the operator has access to the commands of the most "deep"
VIEW, as well as the commands of all higher levels. You can use the
navigation commands to exit nested levels. When changing the current
path, the navigation command used determines whether the VIEW of the
current path is preempted at the same nesting level by another VIEW, or
the new VIEW will become nested and another level will appear in the
current path. How VIEWs defined in XML files do not affect whether a
VIEW can be nested.

When defining VIEWs in XML files, VIEWs can be nested within other
VIEWs.  Do not confuse this nesting with nesting when forming the
current path.  A VIEW defined inside another VIEW adds its commands to
the commands parent VIEW, but it can also be addressed separately from
the parent.  In addition, there are links to VIEW. By declaring such a
reference inside VIEW, we add the commands of the VIEW pointed to by the
link to the commands of the current VIEW. You can define a VIEW with
"standard" commands and include a reference to this VIEW to other VIEWs
where these commands are required without redefining them.

XML configuration files use the `VIEW` tag to declare a VIEW.


### Commands and Options

Commands (the `COMMAND` tag) can have parameters (the `PARAM` tag).  The
team is determined inside a VIEW and belongs to it.  Parameters are
defined within the command and, in turn, belong to her.  A command can
only consist of one word, unlike the commands in previous versions of
klish.  If you need to define a verbose command, nested commands are
created. Those. within a command identified by the first word of a
verbose command, the command identified by the second word of the
verbose command, and so on.

Strictly speaking, a command differs from a parameter only in that its
value can be only a predefined word, while the parameter value can be
anything. Only the parameter type determines its possible values. So
Thus, a command can be viewed as a parameter with a type that says that
the parameter name itself is a valid value. Internal Implementation
teams like this.

In general, the parameter can be defined directly in VIEW, and not inside
commands, but this is rather an atypical case.

Like VIEW, commands and options can be links. In this case, the link can
be treated simply as a substitution of the object pointed to by the
reference.

Parameters can be required, optional, or required.  choice among several
parameters - candidates. So when you enter the command some parameters
can be specified by the operator, and some not. When parsing command
line, a sequence of selected parameters is compiled.


### Parameter type

The parameter type determines the valid values ​​for that parameter. Usually types
are defined separately using the `PTYPE` tag, and the parameter refers to a specific
type by its name. Also, the type can be defined directly inside the parameter, but this
not a typical case, because standard types manage to cover most of the
needs.

The PTYPE type, like a command, performs a specific action specified by the tag
`ACTION`. The action specified for the type validates the value entered by the operator
parameter and returns the result - success or error.


### Action

Each command must define the action to be taken when that command is
entered.  operator. An action can be one or more actions for one
command.  An action is declared with an `ACTION` tag within a
command. The ACTION is referenced to a symbol (function) from the
function plugin that will be executed in this case.  All data inside the
ACTION tag is available to the symbol. Symbol at will use this
information. For example, the data can be given the script to be
executed by the symbol.

The result of the action is a "return code". It defines success or the
failure of the command as a whole. If one command is defined more than
one action, then the calculation of the return code becomes more complex
task. Each action has a flag that determines whether the return code
affects current action to a common return code. Actions also have
settings, determining whether an action will be performed provided that
the previous action ended with an error. If several actions are
performed in succession, having a flag of influence on the common return
code, then the common return code will be the code return the last such
action.


### Filters

Filters are commands that process the output of other commands.  The
filter can be specified on the command line after the main command and
the `|` sign.  The filter cannot be an independent command and be used
without the main one.  commands. An example of a typical filter is the
UNIX equivalent of the `grep` utility.  Filters can be used one after
the other, delimited by `|`, like this done in the shell.

If the command is not a filter, then it cannot be used after the character
`|`.

The filter is specified in configuration files using the `FILTER` tag.


### Parameter containers

The SWITCH and SEQ containers are used to aggregate the parameters
nested in them and define the rules by which the command line is parsed
regarding these options.

By default, it is considered that if inside the command it is defined
sequentially several parameters, then all these parameters must also be
sequentially present on the command line. However, sometimes it becomes
necessary to the operator entered only one parameter of choice from a
set of possible parameters.  In such a case, the SWITCH container can be
used. If, when parsing a command line inside the command meets the
SWITCH container, then to match the next word entered by the operator
must have only one option selected from the parameters nested in
SWITCH. Those. using the SWITCH container happens parsing branch.

The SEQ container defines a sequence of parameters. All of them should
be sequentially mapped to the command line. Although, as noted earlier,
the parameters nested in the command are already parsed sequentially,
however container can be useful in more complex designs. For example
container SEQ itself can be an element of a SWITCH container.


### Command line prompts

To form the command line prompt that the operator sees, the `PROMPT`
construct is used.  The PROMPT tag must be nested inside the VIEW.
Different VIEWs may have different prompts. Those. depending on the
current path, the operator sees a different prompt. Prompt can be
dynamic and be generated by `ACTION` actions defined inside PROMPT.


### Completion and hints

For the convenience of the operator for commands and parameters can be
implemented hints and autocomplete. Tips (help) explaining the purpose
and format of possible parameters, displayed in the klish client on
keystroke `?`. The list of possible additions is printed when the
operator presses the `Tab` key.

To specify hints and a list of possible additions to the configuration,
use the `HELP` and `COMPL` tags. These tags must be nested relatively
corresponding commands and parameters. For ease of hinting for the
parameter or commands can be given by the main tag attribute if the hint
is static text and does not need to be generated. If the prompt is
dynamic, then its content is generated by ACTION actions nested inside
HELP. For COMPL additions to the ACTION action generate a list of
possible values parameters, separated by a newline.


### Conditional element

Commands and parameters can be hidden from the operator based on dynamic
conditions. The condition is specified using the `COND` tag nested
inside corresponding command or parameter. Inside COND are one or more
action. If the return code of ACTION is `0`, i.e. success, then the
option is available operator. If ACTION returned an error, then the
element will be hidden.


### Plugin

The klishd process does not load all existing function plugins in a row,
but only those specified in the configuration using the `PLUGIN`
tag. Data inside the tag can be interpreted by the plugin initialization
function at their own discretion, in particular, as a configuration for
a plugin.


### Hotkeys

For the convenience of the operator, "hot keys" can be specified in the
klish command configuration.  hotkeys. Tag for setting hotkeys -
`HOTKEY`. List of hotkeys sent by the klishd server to the client. The
client uses this information or does not use. For example, for an
automated control client the hotkey information is meaningless.

When defining a hot key, a text command is specified, which should be
executed when the hotkey is pressed by the operator. It might be fast
displaying the current device configuration, exiting the current VIEW,
or any other team. The klish client "catches" the hotkey press and sends
it to the server command corresponding to the hotkey pressed.


### Links to elements

Some tags have attributes that are links to others defined by in
configuration files, items. For example `VIEW` has a `ref` attribute,
with which a "copy" of a third-party `VIEW` is created at the current
location of the schema. or tag `PARAM` has a `ptype` attribute that
refers to a `PTYPE`, specifying parameter type. Klish has its own
"language" for organizing links. Can to say this is a highly simplified
analogue of paths in the file system or XPath.

The path to an element in klish is typed, specifying all of its parent
elements with delimiter `/`. The name of an element is the value of its
`name` attribute. Path also starts with a `/` character, denoting the
root element.

> Relative paths are not currently supported

```xml
<KLISH>

<PTYPE name="PTYPE1">
	<ACTION sym="sym1"\>
</PTYPE>

<VIEW name="view1">

	<VIEW name="view1_2">
		<COMMAND name="cmd1">
			<PARAM name="par1" ptype="/PTYPE1"/>
		</COMMAND>
	</VIEW>

</VIEW>

<VIEW name="view2">

	<VIEW ref="/view1/view1_2"/>

</VIEW>

</KLISH>
```

The "par1" parameter refers to a `PTYPE` using the path `/PTYPE1`. Type names
it is customary to designate with capital letters to make it easier to distinguish types from others
elements. Here, the type is defined at the topmost level of the schema. Basic types
usually defined this way, but `PTYPE` need not be at the top level and
may be nested in `VIEW` or even `PARAM`. In that case, he will have
more difficult path.

`VIEW` named "view2" imports commands from `VIEW` named "view1_2",
using the path `/view1/view1_2`.

If, suppose, you need a reference to the "par1" parameter, then the path will look like
so - `/view1/view1_2/cmd1/par1`.

The following elements cannot be referenced. They don't have a path:

*`KLISH`
* `PLUGIN`
* `HOTKEY`
*`ACTION`

> Do not confuse [current session path](#scope-scope) with the path to create
> links. The path when creating links uses the internal structure of the schema,
> specified when writing configuration files. This is the path of the element within the schema,
> uniquely identifying it. This is a static path. Element Nesting
> when defining a schema, it only generates the necessary sets of commands, this nesting
> is not visible to the operator and is not a nesting in terms of its work with
> command line. He sees only ready-made linear instruction sets.
>
> The current session path is dynamic. He
> is formed by the operator's commands and implies the ability to go deeper, i.e.
> add another level of nesting to the path and access
> an additional set of commands, or go higher. By using
> of the current path, you can combine the linear ones created at the stage of writing the diagram
> command sets. Navigation commands also allow you to completely replace the current
> command set to another by changing `VIEW` at the current path level. Thus,
> the existence of a current session path may give the operator the appearance
> branched configuration tree.


## Tags


### KLISH

Any XML file with klish configuration must begin with a `KLISH` opening tag
and end with the closing `KLISH` tag.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<KLISH
	xmlns="https://klish.libcode.org/klish3"
	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
	xsi:schemaLocation="https://src.libcode.org/pkun/klish/src/master/klish.xsd">

<!-- Тут любая конфигурация для klish -->

</KLISH>
```

The header introduces its default XML namespace
"https://klish.libcode.org/klish3". The title is standard and most often does not make sense
change it.


### PLUGIN

By itself, klish does not load any executable function plugins.
Accordingly, no symbols are available to the user for use in actions.
Loading plugins must be explicitly written in files configuration. The
`PLUGIN` tag is used for this.

Note that even the base plugin named "klish" is also not loaded
automatically and it must be registered in the configuration files. In
this plugin, in particular, navigation is implemented. A typical
configuration would contain the line:

```xml
<PLUGIN name="klish"/>
```

The `PLUGIN` tag cannot contain other subtags.


#### The `name` attribute

The attribute defines the name by which the plugin will be recognized
inside files configuration. When `ACTION` refers to a symbol, it can be
specified simply the name of the symbol, or it can be specified in which
plug-in to search for the symbol.

```xml
<ACTION sym="my_sym"\>

<ACTION sym="my_sym@my_plugin"\>
```

In the first case klish will look for "my_sym" in all plugins and use
the first found. In the second case, the search will be performed only
in the plugin "my_plugin". In addition, different plugins may contain
symbols of the same name and specifying the plugin will let you know
which symbol was meant.

If the `id` and `file` attributes are not specified, then `name` is also
used for formation of the plugin file name. The plugin must be named
`kplugin-.so` and be in the `/usr/lib/klish/plugins` directory (if klish
was configured with `--prefix=/usr`).


#### The `id` attribute

The attribute is used to form the plugin filename if the attribute is
not specified.  `file`. The plugin must be named `kplugin-.so` and be in
the directory `/usr/lib/klish/plugins` (if klish was configured with
`--prefix=/usr`).

If the `id` attribute is specified, then `name` will not be used to form
plugin file name, but only for identification within configuration files.

```xml
<PLUGIN name="alias_for_klish" id="klish"\>

<ACTION sym="nav@alias_for_klish"\>
```

#### The `file` attribute

The fully qualified file name of the plugin (shared library) can be
specified explicitly. If the `file` attribute is specified, then no
other attributes will be used to form the plugin filename, but the value
of `file` "as is" will be taken and passed to the `dlopen()`
function. This means that either an absolute path or just a file name
can be specified, but in that case the file must be in the standard
paths used when searching for a shared library.

```xml
<PLUGIN name="my_plugin" file="/home/ttt/my_plugin.so"\>

<ACTION sym="my_sym@my_plugin"\>
```

#### Data inside the tag


Data inside the `PLUGIN` tag can be processed by the plugin's
initialization function. The data format is up to the plugin itself. For
example, settings for a plug-in can be specified as data.

```xml
<PLUGIN name="sysrepo">
	JuniperLikeShow = y
	FirstKeyWithStatement = n
	MultiKeysWithStatement = y
	Colorize = y
</PLUGIN>
```

### HOTKEY

For more convenient work in the command line interface, "hot keys" can
be set for frequently used commands. A hotkey is defined using the
`HOTKEY` tag. Hotkeys require support in the client program that
connects to the klishd server. The "klish" client has such support. The
`HOTKEY` tag cannot have nested tags. Any additional data inside the tag
is not provided.

#### The `key` attribute

This attribute specifies what the operator must press to activate the
hotkey. Combinations with "Ctrl" key are supported. For example `^Z`
means that the key combination "Ctrl" and "Z" should be pressed.

```xml
<HOTKEY key="^Z" cmd="exit"\>
```

#### The `cmd` attribute

The attribute defines what action will be performed when the hot key is
pressed by the operator. The value of the `cmd` attribute is parsed
according to the same rules as any other command manually entered by the
operator.

```xml
<COMMAND name="exit" help="Exit view or shell">
	<ACTION sym="nav">pop</ACTION>
</COMMAND>

<HOTKEY key="^Z" cmd="exit"\>
```

The command used as the value of the `cmd` attribute must be defined in
the configuration files. The above example will execute the previously
defined `exit` command when the "Ctrl^Z" key combination is pressed.


### ACTION

The `ACTION` tag defines the action to be performed. A typical use of
the tag is within a `COMMAND` command. When the operator enters the
appropriate command, the actions defined in `ACTION` will be performed.

The main attribute of `ACTION` is `sym`. An action can only perform
symbols (functions) defined in plugins. The `sym` attribute refers to
such a symbol.

Actions can be performed by more than just a command. The following is a
list of tags within which `ACTION` may occur:

 * `ENTRY` - what `ACTION` will be used for is determined by the `ENTRY`
   parameters.
 * `COMMAND` - action is performed, defined in `ACTION` when the
   operator enters the corresponding command.
 * `PARAM` - same as for `COMMAND`.
 * `PTYPE` - `ACTION` defines actions to check the value of an
   operator-entered parameter that has the corresponding type.
 
Within the listed elements, there can be several `ACTION` elements at
the same time. Let's call it a block of `ACTION` elements. Actions are
executed sequentially, one after the other, unless otherwise specified
by the `exec_on` attribute.

Within one command, several action blocks can be defined. This is
possible if the command has parameter branching or optional
parameters. A block is an action defined within a single
element. Actions defined in different elements, including nested ones,
belong to different blocks. Only one block of actions is always
executed.

```xml
<COMMAND name="cmd1">
	<ACTION sym="sym1"\>
	<SWITCH min="0">
		<COMMAND name="opt1">
			<ACTION sym="sym2"\>
		</COMMAND>
		<COMMAND name="opt2"\>
		<PARAM name="opt3" ptype="/STRING">
			<ACTION sym="sym3"\>
		</PARAM>
	</SWITCH>
</COMMAND>
```

In the example, the "cmd1" command is declared, which has three
alternative (only one of the three can be specified) optional
parameters. The search for actions to perform goes from the end to the
beginning when parsing the entered command line.

So if the operator entered the command `cmd1`, then the parsing engine
will recognize the command named "cmd1" and will look `ACTION` directly
at that element. Will be found `ACTION` with the symbol "sym1".

If the operator entered the command `cmd1 opt1`, then the string "opt1"
will be recognized as a parameter (aka subcommand) with the name
"opt1". The search is from the end, so it will be found `ACTION` with
the symbol "sym2" first. Once the action block is found, no more
actions will be searched and "sym1" will not be found.

If the operator entered the command `cmd1 opt2`, then the action with
the symbol "sym1" will be found, since the "opt2" element does not have
its own nested actions and the search goes up to the parent elements.

If the operator entered the command `cmd1 arbitrary_string`, then the
action with the symbol "sym3" will be found.

#### Attribute `sym`

The attribute refers to a symbol (function) from the plugin. This
function will be executed when the ACTION. The value can be a symbol
name, such as `sym="my_sym"`. In this case, the symbol will be searched
for all loaded plugins. If a plugin is specified in which to search for
a symbol, for example `sym="my_sym@my_plugin"`, then other plugins will
not be searched.

In different situations, it may be advantageous to take different
approaches as to whether or not to specify a plugin name for a symbol.
On the one hand, several plugins can contain symbols with the same name,
and for the unambiguous identification of a symbol, specifying the
plugin will be mandatory. In addition, when specifying a plugin, the
search for a symbol will be a little faster. On the other hand, you can
write some universal commands that specify symbols without belonging to
the plugin.  Then multiple plugins can implement the "interface",
i.e. all symbols used, and their actual content will depend on which
plugin is loaded.

#### Attribute `lock`

> Attention: the attribute is not implemented yet

Some actions require atomicity and exclusive access to a resource.  When
working in klish, this is not automatically provided.  Two operators can
independently but simultaneously run the same command for execution.
Locks can be used to provide atomicity and/or exclusive access to a
resource `lock`.  Locks in klish are named. The attribute `lock`
specifies the lock with which name to acquire `ACTION` on execution.
For example `lock="my_lock"`, where "my_lock" is the name of the
lock. Acquiring a lock with one name does not affect locks with another
name in any way.  Thus, not one global lock can be implemented in the
system, but several separate ones, based, for example, on which resource
the lock protects.

If a lock is held by one process, then another process attempting to
acquire the same lock will suspend its execution until the lock is
released.

#### Attribute `interactive`

The attribute specifies whether the action is interactive. For example,
starting a text editor is interactive, because when working in the
editor, user actions affect the result and must come from the client
program to the server. Also, changes on the part of the editor should
immediately be displayed to the operator.

Possible attribute values ​​are: `true` and `false`.  By default `false`,
i.e., the action is not interactive.

If the action is declared interactive, then when it is executed, a
pseudo-terminal is created for exchange with the user. It is not
recommended to set the interactive flag for non-interactive actions.

#### Attribute `permanent`

The klish system can be run in "dry-run" mode, where all actions will
not actually be performed, and their return code will always be
"success".  This mode can be used for testing, for checking the
correctness of incoming data, etc.

However, some symbols must always be executed, regardless of the mode.
An example of such a symbol would be a navigation function.  Those, you
should always change the current session path, regardless of the mode of
operation.

The flag `permanent` can change the behavior of the action in "dry-run"
mode. Possible attribute values ​​are: `true` and `false`.  By default
`false`, i.e., the action is not "permanent" and will be disabled in
"dry-run" mode.

Symbols, when declared in a plugin, already have a sign of persistence.
The symbol can be forced to be permanent, it can be forced to be
non-persistent, or the plug-in can leave the persistence decision to the
user. If the persistence flag is forced in the plugin, then setting the
attribute `permanent` will not affect anything.  It cannot override the
persistence flag forced inside the plugin.


#### Attribute `sync`

A symbol can execute "synchronously" or "asynchronously".  In
synchronous mode, the symbol code will be run directly in the context of
the current process - the klishd server.  In asynchronous mode, a
separate process will be spawned (fork()) to run the symbol
code. Running a symbol in asynchronous mode is safer because bugs in the
code won't affect the klishd process. It is recommended to use
asynchronous mode.

Possible attribute values ​​are: `true` and `false`.  By default `false`,
i.e., symbol will be executed asynchronously.

Symbols, when declared in a plugin, already have a sign of synchronism.
The symbol can be forced to be synchronous, asynchronous, or the plugin
can leave the decision about synchronism to the user. If the persistence
flag is forced in the plugin, then setting the attribute `sync` will not
affect anything.  It cannot override the sync flag forced inside the
plugin.

#### Attribute `update_retcode`

One command can contain several elements `ACTION`. This is called an
"action block". Each action has its own return code. However, the
command as a whole must also have a return code, and this code must be a
single value, not an array.

By default, actions `ACTION` are executed sequentially, and as soon as
one of the actions returns an error, block execution is stopped and an
error is considered a common return code. If no action from the block
returned an error, then the return code of the last action in the block
is considered the return code.

Sometimes it is required that, regardless of the return code of a
certain action, the execution of the block continues. The attribute can
be used for this `update_retcode`.  The attribute can take the value
`true` or `false`.  The default is `true`.  This means that the return
code of the current activity affects the overall return code.  At this
point, the global return code will be set to the value of the current
return code. If the flag value is set to `false`, then the current
return code is ignored and will not affect the formation of the general
return code in any way.  Also, the execution of the block of actions
will not be interrupted in case of an error at the stage of executing
the current action.

#### Attribute `exec_on`

When executing a block of actions (several `ACTION` within one element),
the actions are executed sequentially until all actions are completed,
or until one of the actions returns an error.  In this case, the
execution of the block is interrupted. However, this behavior can be
controlled by the `exec_on`.  The attribute can take the following
values:

 * `success` - the current action will be sent for execution if the
   value of the common return code at the moment is "success".
 * `fail` - the current action will be sent for execution if the value
   of the common return code at the moment is "error".
 * `always` - the current action will be executed regardless of the
   common return code.
 * `never` - the action will not be performed under any circumstances.

The default value is `success`, i.e. actions are performed if there were
no errors before.

```xml
<ACTION sym="printl">1</ACTION>
<ACTION sym="printl" exec_on="never">2</ACTION>
<ACTION sym="printl">3</ACTION>
<ACTION sym="printl" exec_on="fail">4</ACTION>
<ACTION sym="script">/bin/false</ACTION>
<ACTION sym="printl">6</ACTION>
<ACTION sym="printl" exec_on="fail" update_retcode="false">7</ACTION>
<ACTION sym="printl" exec_on="always">8</ACTION>
<ACTION sym="printl" exec_on="fail">9</ACTION>
```

This example will display:

```xml
1
3
7
8
```

The string "1" will be printed because at the beginning of the action
block execution, the general return code is assumed to be "success" and
the `exec_on` default value is `success`.

The string "2" will not be displayed because `on_exec="never"`, i.e. do
not perform under any circumstances.

Line "3" will be executed because the previous action (line "1") was
successful.

Line "4" will fail because the condition is set `on_exec="fail"`, and the
previous action "3" succeeded and set the common return code to
"success".

Line "5" will execute and translate the generic return code into an
"error" value.

Line "6" will fail because the current generic return code equals
"error" and the line should only execute if the generic return code
succeeds.

The string "7" will be output because the condition is `on_exec="fail"`,
the current generic return code is indeed "error".  Note that although
the action itself succeeds, the generic return code will not be changed
because the `update_retcode="false"`.

The string "8" will be printed because the condition is set
`on_exec="always"`, which means to perform the action regardless of the
current common return code.

Line "9" will not be output because line "8" changed the common return
code to "success".

#### Data inside the tag

The data inside the `ACTION` tag is used at the discretion of the symbol
itself, specified by the attribute sym.  An example is the symbol
`script` from the plugin script.  This symbol uses the data inside the
tag as the code for the script it should execute.

```xml
<COMMAND name="ls" help="List root dir">
	<ACTION sym="script">
	ls /
	</ACTION>
</COMMAND>

<COMMAND name="pytest" help="Test for Python script">
	<ACTION sym="script">#!/usr/bin/python
	import os
	print('ENV', os.getenv("KLISH_COMMAND"))
	</ACTION>
</COMMAND>
```

Note that the "pytest" command has a shebang `#!/usr/bin/python` that
specifies which interpreter to run the script with.


### ENTRY

> Typically, the `ENTRY` tag is not explicitly used in configuration
> files.  However, the tag is the base tag for most other tags and most
> of its attributes are inherited.

If you look at the internal implementation of klish, then you will not
find the whole set of tags available when writing an XML configuration.
In fact, there is a base element `ENTRY` that implements the
functionality of most other tags.  The element "turns" into other tags
depending on the value of its attributes.  The following tags are
internally implemented as an element `ENTRY`:

 * `VIEW`
 * `COMMAND`
 * `FILTER`
 * `PARAM`
 * `PTYPE`
 * `COND`
 * `HELP`
 * `COMPL`
 * `PROMPT`
 * `SWITCH`
 * `SEQ`

This section will go into some detail about the attributes of the
element `ENTRY`, which are often attributes of other elements as well.
Other elements will refer to these descriptions in the `ENTRY`.
Configuration examples, when describing attributes, are not necessarily
based on the element `ENTRY`, but use other, more typical "wrapper"
tags.

The element is based `ENTRY` on attributes that determine the features of
its behavior and the ability to nest `ENTRY` other elements inside the
element `ENTRY`.  Thus, the entire configuration scheme is built.

#### Attribute `name`

The attribute `name` is an element identifier. Among the elements of the
current nesting level, the identifier must be unique. Elements with the
same name can be present in different schema branches. It is important
that the absolute path of the element is unique, i.e. a combination of
the name of the element itself and the names of all its "ancestors".

For a tag `COMMAND`, the attribute also serves as the value of the
positional parameter if the attribute is not defined `value`. Those. the
operator enters a string equal to the element name `COMMAND` to invoke
this command.

#### Attribute `value`

If the command identifier (attribute `name`) differs from the command
name for the operator, then the attribute `value` contains the command
name as it appears to the operator.

Used for the following tags:

 * `COMMAND`
 * `PARAM`
 * `PTYPE`

```xml
<COMMAND name="cmdid" value="next"\>
```

In the example, the command ID is "cmdid". It will be used if you need
to create a link to this element inside XML configs. But the user, in
order to run the command for execution, enters the text on the command
line `next`.

#### Attribute `help`

When working with the command line, the operator can get a hint on
possible commands, parameters and their purpose.  In the "klish" client,
a tooltip will be shown when the key `?` is pressed.  The easiest way to
set tooltip text for an element is to specify the value of the attribute
`help`.

The following tags support the attribute help:

 * `COMMAND`
 * `PARAM`
 * `PTYPE`

A tooltip specified with the attribute `help` is static.  Another way to
set a tooltip for an element is to create a nested element `HELP`.  The
element `HELP` generates tooltip text dynamically.

```xml
<COMMAND name="simple" help="Command with static help"/>

<COMMAND name="dyn">
	<HELP>
		<ACTION sym="script">
		ls -la "/etc/passwd"
		</ACTION>
	<HELP>
</COMMAND>
```

If an element has both an attribute `help` and a nested element `HELP`,
then the dynamic nested element will be used `HELP` and the attribute
will be ignored.

The element `PTYPE` has its hints.  Both a static attribute and a
dynamic element.  These hints will be used for a parameter `PARAM` that
uses this data type if neither the attribute nor the dynamic element is
set for the parameter `HELP`.


#### Attribute `container`

A "container" in klish terms is an element that contains other elements
but is itself not visible to the operator.  Those, this element is not
mapped to any positional parameters when parsing the entered command
line. It only organizes other elements. An example of containers are
`VIEW`, `SWITCH`, `SEQ`.  The tag `VIEW` organizes the commands in
scope, but the element itself is not a command or parameter to the
operator.  The operator cannot enter the name of this element on the
command line, he can immediately refer to the elements nested in the
container (if they are not containers themselves).

The elements `SWITCH` and `SEQ` are also not visible to the operator.
They define how nested elements should be handled.

The attribute `container` can take the values `true​​` or `false`.  The
default is `false`, i.e., element is not a container.  At the same time,
it should be noted that for all wrapper elements inherited from `ENTRY`,
the attribute value is pre-written to the correct value.  Those, it is
usually not necessary to use the attribute in real configs.  Only in
specific cases is it really required.

#### Attribute `mode`

The attribute `mode` determines how nested elements will be processed
when parsing the entered command line.  Possible values:

 * `sequence` - nested elements will be compared to the "words" entered
   by the operator sequentially, one after the other. Thus, all nested
   elements can take on values ​​when parsed.
 * `switch` - only one of the nested elements must be matched to input
   from the operator. This is one of many choices. Items that are not
   selected will not receive values.
 * `empty` - element cannot have nested elements.

So the element `VIEW` is forced to have the attribute `mode="switch"`,
assuming that it contains commands within itself and it should not be
possible to enter many commands at once one after another in one line.
The operator selects only one command at a time.

The `COMMAND` and `PARAM` elements have a setting by default
`mode="sequence"`, since most often they contain parameters that must be
entered one after the other.  However, it is possible to override the
attribute `mode` in the tags `COMMAND`, `PARAM`.

Elements `SEQ` are `SWITCH` containers and are designed only to change
how nested elements are handled.  The element `SEQ` has
`mode="sequence"`, the element `SWITCH` has `mode="switch"`.

The following are examples of branches within a team:

```xml
<!-- Example 1 -->
<COMMAND name="cmd1">
	<PARAM name="param1" ptype="/INT"/>
	<PARAM name="param2" ptype="/STRING"/>
</COMMAND>
```

The default command is `mod="sequence"`, so the operator will have to
enter both parameters, one after the other.

```xml
<!-- Example 2 -->
<COMMAND name="cmd2" mode="switch">
	<PARAM name="param1" ptype="/INT"/>
	<PARAM name="param2" ptype="/STRING"/>
</COMMAND>
```

The attribute value `mode` is overridden, so the operator will only have
to enter one of the parameters.  Which of the parameters correspond to
the entered characters in this case will be determined as follows.
First, the first parameter "param1" is checked for correctness.  If the
string matches the format of an integer, then the parameter takes its
value and the second parameter is not checked for compliance, although
it would also fit, since the type `STRING` can contain numbers.  Thus,
when selecting one option out of many, the order in which the options
are specified is important.

```xml
<!-- Example 3 -->
<COMMAND name="cmd3">
	<SWITCH>
		<PARAM name="param1" ptype="/INT"/>
		<PARAM name="param2" ptype="/STRING"/>
	</SWITCH>
</COMMAND>
```

This example is identical to example "2". Only `mode` the nested tag is
used instead of the attribute `SWITCH`.  The entry in the example "2" is
shorter, in the example "3" it is clearer.

```xml
<!-- Example 4 -->
<COMMAND name="cmd4">
	<SWITCH>
		<PARAM name="param1" ptype="/INT">
			<PARAM name="param3" ptype="/STRING">
			<PARAM name="param4" ptype="/STRING">
		</PARAM>
		<PARAM name="param2" ptype="/STRING"/>
	</SWITCH>
	<PARAM name="param5" ptype="/STRING"/>
</COMMAND>
```

This demonstrates how command line arguments are parsed.  If the
parameter "param1" is selected, then its nested elements "param3" and
"param4" are used, and then only `SWITCH` the element "param5" following
the element is used. The parameter "param2" is not used in any way.

If "param2" was selected first, then "param5" is processed and the process ends.

```xml
<!-- Example 5 -->
<COMMAND name="cmd5">
	<SWITCH>
		<SEQ>
			<PARAM name="param1" ptype="/INT">
			<PARAM name="param3" ptype="/STRING">
			<PARAM name="param4" ptype="/STRING">
		</SEQ>
		<PARAM name="param2" ptype="/STRING"/>
	</SWITCH>
	<PARAM name="param5" ptype="/STRING"/>
</COMMAND>
```

The example is completely similar to the behavior of example "4".  Only
instead of nesting, the tag is used `SEQ`, which says that if the first
parameter from the block of sequential parameters has already been
selected, then the rest must also be entered one by one.

```xml
<!-- Example 6 -->
<COMMAND name="cmd6">
	<COMMAND name="cmd6_1">
		<PARAM name="param3" ptype="/STRING">
	</COMMAND>
	<PARAM name="param1" ptype="/INT"/>
	<PARAM name="param2" ptype="/STRING"/>
</COMMAND>
```

The example shows a nested "subcommand" named "cmd1_6". Here, the
subcommand is no different from the parameter, except that the criterion
for matching command-line arguments to a command `COMMAND` is that the
supplied argument matches the command name.

#### Attribute `purpose`

Some nested elements must have a special meaning. For example, an
element can be defined inside VIEWthat generates an invitation text for
the operator. To separate an element for generating a prompt from nested
commands, you need to give it a special attribute. Later, when the
klishd server needs to get the user's prompt to do so VIEW, the code
will look at the nested elements VIEWand select the element that is
specifically designed for this.

An attribute `purpose` assigns a special purpose to an element.  Possible
appointments:

 * `common` - there is no special purpose. Regular tags have exactly
   this attribute value.
 * `ptype` - the element defines the type of the parent parameter. Tag
   `PTYPE`.
 * prompt- the element is used to generate a user prompt for the parent
   element. Tag PROMPT. The parent element is `VIEW`.
 * `cond` - the element checks the condition and, in case of failure,
   the parent element becomes unavailable to the operator. Tag
   `COND`. Currently not implemented.
 * `completion` - the element generates possible completions for the
   parent element. Tag `COMPL`.
 * `help` - the element generates a tooltip for the parent element. Tag
   `HELP`.

Typically, the attribute `purpose` is not used directly in configuration
files, since each special purpose has its own tag, which is more
descriptive.

#### Attribute `ref`

A schema element can be a reference to another schema element.  Creating
an element "#1" that is a reference to element "#2" is equivalent to
having element "#2" declared at the location in the schema where element
"#1" is located.  It would be more accurate to say that this is
equivalent to making a copy of the element "#2" in the place where the
element "#1" is defined.

If the element is a link, then the attribute is defined in it ref.
Attribute value reference to the target schema element.

```xml
<KLISH>

<PTYPE name="ptype1">
	<ACTION sym="INT"/>
</PTYPE>

<VIEW name="view1">
	<COMMAND name="cmd1"/>
</VIEW>

<VIEW name="view2">
	<VIEW ref="/view1"/>
	<COMMAND name="cmd2">
		<PARAM name="param1" ptype="/ptype1"/>
		<PARAM name="param2">
			<PTYPE ref="/ptype1"/>
		</PARAM>
	</COMMAND>
</VIEW>

<VIEW name="view3">
	<COMMAND ref="/view2/cmd2"/>
</VIEW>

<VIEW name="view4">
	<COMMAND name="do">
		<VIEW ref="/view1"/>
	</COMMAND>
</VIEW>

</KLISH>
```

In the example, "view2" contains a reference to "view1", which is
equivalent to declaring a copy of "view1" inside "view2". And this in
turn means that the "cmd1" command will become available in "view2".

Another `VIEW` named "view3" declares a reference to the "cmd2"
command. Thus a separate command becomes available inside "view3".

The parameters "param1" and "param2" have the same type `/ptype1`.  A
type reference can be written with an attribute ptype, or with a nested
tag `PTYPE` that is a reference to a previously declared `PTYPE`.  As a
result, the types of the two declared parameters are completely
equivalent.

In the last example, the link to `VIEW` is wrapped inside the tag
`COMMAND`.  In this case, this will mean that if we are working with
"view4", then the commands from "view1" will be available with the "do"
prefix. Those. if the operator is in `VIEW` the name "view4", then he
must write on the command line do `cmd1` to execute the command "cmd1".

Links help you reuse code.  For example, declare a block of "standard"
parameters and then use links to insert this block into all commands
where the parameters are repeated.


#### Attribute `restore`

Suppose the current path consists of several levels. Those. operator
entered nested `VIEW`. Being in a nested `VIEW`, the operator has access
to commands not only of the current VIEW, but also of all parent
ones. The operator enters the command defined in the parent VIEW. The
attribute restoredetermines whether the current path will be changed
before the command is executed.

Possible attribute values ​​are `restore` - `true` or `false`. The default
is `false`. This means that the command will be executed and the current
path will remain the same. If `restore="true"`, then the current path
will be changed before the command is executed. Nested levels of the
path will be taken down to the level of the one `VIEW` where the entered
command is defined.

The attribute restoreis used on the element `COMMAND`.

The "native" path recovery behavior of the command can be used on a
system where the configuration mode is implemented in a manner similar
to "Cisco" routers. In this mode, the transition to one section of the
configuration is possible without leaving in advance from another -
neighboring section. To do this, you must first change the current path,
rise to the level of the command being entered, and then immediately go
to the new section, but based on the current path corresponding to the
command to enter the new section.

#### Attribute `order`

The attribute `order` determines whether the order is important when
entering optional parameters declared one after another.  Suppose three
consecutive optional parameters are declared.  By default
`order="false"`, this means that the operator can enter these three
parameters in any order.  Let's say first the third, then the first and
then the second.  If the flag is on the second parameter `order="true"`,
then by entering the second parameter first, the operator will no longer
be able to enter the first one after it. Only the third option will be
available to him.

The attribute orderis used on the element `PARAM`.

#### Attribute `filter`

> Attention: Filters do not work yet.

Some commands are filters. This means that they cannot be used on their
own. Filters only process the output of other commands.  Filters are
specified after the main command and the delimiter character `|`.  The
filter cannot be used before the first character `|`. However, a
non-filter command cannot be used after the `|`.

The attribute `filter` can take the values `true` ​​or `false`.  The
default is `filter="false"`.  This means that the command is not a
filter.  If `filter="true"`, then the command is declared as a filter.

The attribute `filter` is rarely used explicitly in configs. A tag has
been introduced `FILTER` that declares a command that is a filter. Apart
from these restrictions on the use of filters, filters are no different
from regular commands.

A typical example of a filter is the standard "grep" utility.

#### Attributes `min` and `max`

The `min` and `max` attributes are used on an element `PARAM` and
determine how many arguments entered by the operator can be matched to
the current parameter.

The attribute `min` defines the minimum number of arguments that must
match the parameter, i.e. must be validated against the data type of
this parameter.  If `min="0"`, then the parameter becomes optional.
Those. if it is not entered, then it is not an error. The default is
accepted `min="1"`.

The attribute `max` specifies the maximum number of arguments of the
same type that can be matched to the parameter. If the operator enters
more arguments than specified in the attribute max, then those arguments
will not be checked against the current parameter, but may be checked
against other parameters defined after the current one. The default is
accepted `max="1"`.

The attribute `min="0"` can be used on the element `COMMAND` to declare a
subcommand optional.

```xml
<PARAM name="param1" ptype="/INT" min="0"/>
<PARAM name="param2" ptype="/INT" min="3" max="3"/>
<PARAM name="param3" ptype="/INT" min="2" max="5"/>
<COMMAND name="subcommand" min="0"/>
```

In the example, the "param1" parameter is declared optional. The
parameter "param2" must match exactly three arguments entered by the
operator. The parameter "param3" can have two to five arguments. The
"subcommand" subcommand is declared optional.


### VIEW

`VIEW` are designed to organize commands and other configuration
elements in a "scope".  When running a klish statement, there is a
current session path.  The elements of the path are the elements VIEW.
You can change the current path using navigation commands.  If the
operator is in a nested `VIEW`, i.e. the current session path contains
several levels, like nested directories in the file system, the operator
has access to all commands belonging not only to the one `VIEW` located
at the highest level of the path, but also to all the "previous" ones
`VIEW` that make up the path.

It is possible to create "shadow" `VIEWs` that will never be elements in
the current path.  These `VIEWs` can be referenced and thus add their
content to the place in the schema where the link is created.

`VIEW` can be defined within the following elements:

 * `KLISH`
 * `VIEW`
 * `COMMAND`
 * `PARAM`

#### Element attributes

 * `name` - element identifier
 * `help` - description of the element
 * `ref` - link to another `VIEW`

#### Examples

```xml
<VIEW name="view1">
	<COMMAND name="cmd1"/>
	<VIEW name="view1_2">
		<COMMAND name="cmd2"/>
	</VIEW>
</VIEW>

<VIEW name="view2">
	<COMMAND name="cmd3"/>
	<VIEW ref="/view1"/>
</VIEW>

<VIEW name="view3">
	<COMMAND name="cmd4"/>
	<VIEW ref="/view1/view1_2"/>
</VIEW>

<VIEW name="view4">
	<COMMAND name="cmd5"/>
</VIEW>
```

The example demonstrates how scopes work with respect to the commands
available to the operator.

If the current session path is `/view1`, then the commands "cmd1" and
"cmd2" are available to the operator.

If the current session path is `/view2`, then the commands "cmd1", "cmd2",
"cmd3" are available to the operator.

If the current session path is `/view3`, then the commands "cmd2" and
"cmd4" are available to the operator.

If the current session path is `/view4`, then the "cmd5" command is
available to the operator.

If the current session path is `/view4/view1`, then the commands "cmd1",
"cmd2", "cmd5" are available to the operator.

If the current session path is `/view4/{/view1/view1_2}`, then the
commands "cmd2", "cmd5" are available to the operator.  The curly
brackets here denote the fact that the path element can be any `VIEW`
and it doesn't matter if it is nested in the schema definition.  Inside
the curly braces is the unique path of the element in the schema.  The
[Links to Elements](#links-to-elements) section explains the difference
between a path that uniquely identifies an element within a schema and
the current session path.


### COMMAND

The tag `COMMAND` declares the command.  Commands can be executed by the
operator.  The attribute is used to match the argument entered by the
operator with the command `name`.  If it is required that the identifier
of the command within the scheme is different from the name of the
command as it is presented to the operator, then its `name` will contain
an internal identifier, and the attribute `value` "custom" command
name. Attributes `name` and `value` can contain only one word, no spaces.

A command is not much different from an element `PARAM`.  The command may
contain subcommands.  Those. within one command another command can be
declared which is actually a parameter of the first command, only that
parameter is identified by a fixed text string.

A typical command contains a prompt `help` or `HELP` and optionally a
set of nested options `PARAM`. Also, the command must specify the actions
`ACTION` that it performs.

#### Element attributes

 * `name` - element identifier.
 * `value` - "custom" command name.
 * `help` - description of the element.
 * `mode` - mode of processing nested elements.
 * `min` - the minimum number of command line arguments to match the command name.
 * `max` - the maximum number of command line arguments to match the command name.
 * `restore` - flag for restoring the "native" command level in the current session path.
 * `ref` - link to another COMMAND.

#### Example

```xml
<PTYPE name="ptype1">
	<ACTION sym="INT"/>
</PTYPE>

<VIEW name="view1">

	<COMMAND name="cmd1" help="First command">
		<PARAM name="param1" ptype="/ptype1"/>
		<ACTION sym="sym1"/>
	</COMMAND>

	<COMMAND name="cmd2">
		<HELP>
			<ACTION sym="script">
			echo "Second command"
			</ACTION>
		</HELP>
		<COMMAND name="cmd2_1" value="sub2" min="0" help="Subcommand">
			<PARAM name="param1" ptype="ptype1" help="Par 1"/>
		</COMMAND>
		<COMMAND ref="/view1/cmd1"/>
		<PARAM name="param2" ptype="ptype1" help="Par 2"/>
		<ACTION sym="sym2"/>
	</COMMAND>

</VIEW>
```

The command "cmd1" is the simplest version of the command with a hint,
one required parameter of type "ptype1" and an action to be performed.

The "cmd2" command is more complex. The hint is generated dynamically.
The first parameter is an optional subcommand with a "custom" name
"sub2" and one required nested parameter.  Those. an operator wishing to
use a subcommand must begin his command line in this way `cmd2 sub2
....` If the optional "cmd2_1" subcommand is used, then the operator
must specify the value of its required parameter.  The second subcommand
is a link to another command.  To understand what this will mean, it is
enough to imagine that at this place the command to which the link
points is fully described, i.e. cmd1.  Subcommands are followed by a
required numeric parameter and the action performed by the command.


### FILTER

Filter is a command `COMMAND` with the difference that the filter
command cannot be used on its own.  It only handles the output of other
commands and can be used after the main command and the separator
character `|`.  The use of filters is described in more detail in the
"Filters" section.

For the tag `FILTER`, the attribute `filter` is forced to the value `true`.
The remaining attributes and features of operation are the same as the
element `COMMAND`.


### PARAM

The element `PARAM` describes a command parameter.  The parameter has a
type that is specified by either the attribute `ptype` or the subelement
`PTYPE`.  When an operator enters an argument, its value is checked by
the corresponding code `PTYPE` for correctness.

In general, the value for a parameter can be either a string without
spaces or a string with spaces enclosed in quotation marks.

#### Element attributes

 * `name` - element identifier.
 * `value` - an arbitrary value that can be parsed by the code
   `PTYPE`. For the element `COMMAND`, which is a special case of the
   parameter, this field is used as the name of the "custom" command.
 * `help` - description of the element.
 * `mode` - mode of processing nested elements.
 * `min` - the minimum number of command line arguments to match the parameter.
 * `max` - the maximum number of command line arguments to match the parameter.
 * `order` - processing mode of optional parameters.
 * `ref` - link to another `COMMAND`.
 * `ptype` - reference to the parameter type.

#### Example

```xml
<PTYPE name="ptype1">
	<ACTION sym="INT"/>
</PTYPE>

<VIEW name="view1">

	<COMMAND name="cmd1" help="First command">

		<PARAM name="param1" ptype="/ptype1" help="Param 1"/>

		<PARAM name="param2" help="Param 2">
			<PTYPE ref="/ptype1"/>
		</PARAM>

		<PARAM name="param3" help="Param 3">
			<PTYPE>
				<ACTION sym="INT"/>
			</PTYPE>
		</PARAM>

		<PARAM name="param4" ptype="/ptype1" help="Param 4">
			<PARAM name="param5" ptype="/ptype1" help="Param 5"/>
		</PARAM>

		<ACTION sym="sym1"/>
	</COMMAND>

</VIEW>
```

Parameters "param1", "param2", "param3" are identical.  In the first
case, the type is specified by the ptype. In the second, a nested
element `PTYPE` that is a reference to the same type "ptype1". In the
third one, the type of the parameter is determined "on the spot", i.e. a
new type is created `PTYPE`.  But in all three cases, the type is an
integer (see the attribute sym). Specifying the type right inside the
parameter can be handy if the type is not needed anywhere else.

The parameter "param4" has a nested parameter "param5". After entering
an argument for the "param4" parameter, the operator must enter an
argument for the nested parameter.


### SWITCH

The element SWITCH is a container. Its only job is to set the behavior
of nested elements.  `SWITCH` sets such a mode of processing nested
elements, when only one element should be selected from many.

In the `SWITCH` element, attribute `mode` is forced to the value `switch`.

You can read more about nested element processing modes in the
[Attribute mode](#attribute-mode) section.


#### Element attributes

 * `name` - element identifier.
 * `help` - description of the element.

Usually the element `SWITCH` is used without attributes.

#### Example

```xml
<COMMAND name="cmd1" help="First command">
	<SWITCH>
		<COMMAND name="sub1"/>
		<COMMAND name="sub2"/>
		<COMMAND name="sub3"/>
	</SWITCH>
</COMMAND>
```

By default, the element `COMMAND` has the attribute `mode="sequence"`.
If there were no element in the example `SWITCH`, then the operator
would have to sequentially, one after another, specify all subcommands:
`cmd1 sub1 sub2 sub3`.  The element `SWITCH` has changed the nested
element processing mode, and as a result, the operator must select only
one subcommand out of three.  For example `cmd1 sub2`.


### SEQ

The `SEQ` element is a container.  Its only job is to set the behavior
of nested elements.  `SEQ` specifies a nested element processing mode
where all nested elements must be specified one after the other.

In the element, `SEQ` the attribute modeis forced to the value sequence.

You can read more about nested element processing modes in the
[Attribute mode](#attribute-mode) section.

#### Element attributes

 * `name` - element identifier.
 * `help` - description of the element.

Usually the element SEQis used without attributes.

#### Example

```xml
<VIEW name="view1">
	<SEQ>
		<PARAM name="param1" ptype="/ptype1"/>
		<PARAM name="param2" ptype="/ptype1"/>
		<PARAM name="param3" ptype="/ptype1"/>
	</SEQ>
</VIEW>

<VIEW name="view2">
	<COMMAND name="cmd1" help="First command">
		<VIEW ref="/view1">
	</COMMAND>
</VIEW>
```

Suppose we have created a helper `VIEW` containing a list of commonly
used options and named it "view1".  All parameters must be used
sequentially, one after the other.  And then in another `VIEW` they
declared a command that should contain all these parameters.  To do
this, a link to "view1" is created inside the command and all parameters
"fall" into the command. However, the element `VIEW` has an attribute by
default `mode="switch"`, and it turns out that the operator will not
enter all the declared parameters, but must select only one of them. To
change the order in which nested parameters are parsed, the element is
used `SEQ`. It changes the order of parameter parsing to sequential.

The same result could be achieved by adding an attribute
`mode="sequence"` to the "view1" declaration.  Using an attribute is
shorter, using an element `SEQ` is more descriptive.


### PTYPE

The `PTYPE` element describes the data type for the parameters.  The
`PARAM` element refer to a data type using an attribute `ptype` or
contain a nested `PTYPE`.  The task of `PTYPE` is to check the argument
passed by the operator for correctness and return the result in the form
of "success" or "error".  To be more precise, the test result can be
expressed as "suitable" or "not suitable".  `PTYPE` actions are
specified inside the element `ACTION`, which check the argument for
compliance with the declared data type.

#### Element attributes

 * `name` - element identifier.
 * `help` - description of the element.
 * `ref` - link to another `PTYPE`.

#### Example

```xml
<PTYPE name="ptype1" help="Integer">
	<ACTION sym="INT"/>
</PTYPE>

<PARAM name="param1" ptype="/ptype1" help="Param 1"/>
```

The example declares the data type "ptype1".  This is an integer and
character `INT` from the standard "klish" plugin checks that the input
argument is indeed an integer.

See the [PARAM element](#PARAM) Example section for other uses of the
tag `PTYPE` and attribute `ptype`.


### PROMPT

The `PROMPT` element has a special purpose and is nested within the
`VIEW` element.  The purpose of the element is to prompt the user if the
parent `VIEW` is the current session path.  If the current path is
multi-level, then if it does not find an element `PROMPT` in the last
element of the path, the search engine will go up one level and look
`PROMPT` there.  And so on down to the root element.  If it is not found
there , then the standard prompt at the discretion of the klish client
is used.  By default, the klish client uses the prompt `$`.

Within the `PROMPT` element actions are specified in `ACTION` that
define the prompt text for the user.

The `PROMPT` element is forced to have the value of the attribute
`purpose="prompt"`.  It is also a container.

#### Element attributes

 * `name` - element identifier.
 * `help` - description of the element.
 * `ref` - link to another `PROMPT`.

Usually PROMPT is used without attributes.

#### Example

```xml
<VIEW name="main">

	<PROMPT>
		<ACTION sym="prompt">%u@%h&gt; </ACTION>
	</PROMPT>

</VIEW>
```

In the example for `VIEW` named "main", which is the current default
path when klish is run, the user's prompt is defined.  It uses the
symbol `prompt` from the standard "klish" plugin, which helps in the
formation of the prompt, replacing constructions `%u` or `%h`
substitutions, `%u` is replaced by the name of the current user, and
`%h` by the hostname.


### HELP

The `HELP` element has a special purpose and is nested for elements
`COMMAND`, `PARAM`, `PTYPE`.  The purpose of the element is to form the
"help" text, i.e. parent element hint.  Within the element, `HELP`
actions are specified in `ACTION` that form the tooltip text.

The `HELP` element is forced to have the value of the attribute
`purpose="help"`.  It is also a container.

The klish client shows keystroke hints `?`.

#### Element attributes

 * `name` - element identifier.
 * `help` - description of the element.
 * `ref` - link to another `HELP`.

Usually HELP is used without attributes.

#### Example

```xml
<PARAM name="param1" ptype="/ptype1">
	<HELP>
		<ACTION sym="sym1"/>
	</HELP>
</PARAM>
```

### COMPL

The `COMPL` element has a special purpose and is nested for `PARAM` and
`PTYPE`.  The purpose of the element is to generate auto-completion
options, i.e., the possible values ​​for the parent element.  Within the
element, the actions that form the output `COMPL` are specified in
`ACTION`. Individual options in the output are separated from each other
by a line break.

The `COMPL` element is forced to have the value of the attribute
`purpose="completion"`.  It is also a container.

The klish client shows autocomplete suggestions on keypress `Tab`.

#### Element attributes

 * `name` - element identifier.
 * `help` - description of the element.
 * `ref` - link to another `COMPL`.

#### Example

```xml
<PARAM name="proto" ptype="/STRING" help="Protocol">
	<COMPL>
		<ACTION sym="printl">tcp</ACTION>
		<ACTION sym="printl">udp</ACTION>
		<ACTION sym="printl">icmp</ACTION>
	</COMPL>
</PARAM>
```

The parameter specifying the protocol has a type `/STRING` i.e. arbitrary
string.  The operator can enter arbitrary text, but for convenience, the
parameter has auto-completion options.


### COND

> The functionality of the `COND `element has not yet been implemented.

The `COND` element has a special purpose and is nested for elements
`VIEW`, `COMMAND`, and `PARAM`.  The purpose of the element is to hide
the element from the operator if the condition specified in `COND` the
condition is not met.  Within the element, `COND` actions are specified
`ACTION` that check the condition.

The `COND` element is forced to have the value of the attribute
`purpose="cond"`.  It is also a container.

#### Element attributes

 * `name` - element identifier.
 * `help` - description of the element.
 * `ref` - link to another `COND`.

#### Example

```xml
<COMMAND name="cmd1" help="Command 1">
	<COND>
		<ACTION sym="script">
		test -e /tmp/cond_file
		</ACTION>
	</COND>
</COMMAND>
```

If the file `/tmp/cond_file` exists, then the "cmd1" command is
available to the operator, if it does not exist, then the command is
hidden.

### Plugin "klish"

The klish source tree contains the code for the standard "klish" plugin.
The plugin contains basic data types, a navigation command, and other
auxiliary symbols.  In the vast majority of cases, this plugin should be
used.  However, it does not connect automatically, because in some rare
specific cases, you may need to be able to work without it.

The standard way to enable the "klish" plugin in configuration files is:

```xml
<PLUGIN name="klish"/>
```

The plugin comes with a file `ptypes.xml` where the basic data types are
declared as elements [`PTYPE`](#ptype).  Declared data types use plug-in
symbols to check that the argument matches the data type.

### Data types

All section symbols are intended to be used in elements `PTYPE`, unless
otherwise specified, and check that the input argument matches the data
type.

#### Symbol `COMMAND`

The symbol `COMMAND` checks that the supplied argument matches the
command name. Those. with attributes `name` or `value` elements
`COMMAND` or `PARAM`.  If the attribute `value` is set, then its value
is used as the command name.  If not specified, then the value of the
attribute `name` is used.  The case of symbols in the command name is
not taken into account.

#### Symbol `completion_COMMAND`

The symbol `completion_COMMAND` is for an element `COMPL` nested in
`PTYPE`.  The symbol is used to form an autocomplete string for the
command.  Completion for a command is the name of the command itself.
If the attribute `value` is set, then its value is used as the command
name. If not specified, then the value of the attribute `name` is used.

#### Symbol `help_COMMAND`

The symbol `help_COMMAND` is for an element `HELP` nested in `PTYPE`.
The symbol is used to form a description (help) string for the
command.  If the attribute `value` is set, then its value is used as the
command name.  If not specified, then the value of the attribute `name`
is used.  The value of the command attribute is used as the hint itself
help.

#### Symbol `COMMAND_CASE`

The symbol `COMMAND_CASE` is exactly the same as the symbol `COMMAND`,
except that it is case-sensitive in the command name.

#### Symbol INT

Symbol `INT` checks that the input argument is an integer.  The bit
depth of the number corresponds to the type `long long int` in the C
language.  Within the element `ACTION`, a valid range for an integer can
be defined.  The minimum and maximum values ​​are specified, separated by
a space.

```xml
<ACTION sym="INT">-30 80</ACTION>
```

The number can take values ​​in the range from "-30" to "80".

#### Symbol `UINT`

Symbol `UINT` checks that the supplied argument is an unsigned integer.
The bit depth of a number corresponds to the type `unsigned long long
int` in the C language.  Within the element ACTION, a valid range for
the number can be defined. The minimum and maximum values ​​are specified,
separated by a space.

```xml
<ACTION sym="UINT">30 80</ACTION>
```

The number can take values ​​in the range from "30" to "80".

#### Symbol `STRING`

Symbol `STRING` checks that the input argument is a string.  There are
currently no specific string requirements.

### Navigation

Using navigation commands, the operator changes the current path of the
session.

#### Symbol `nav`

The symbol `nav` is for navigation.  With the help of subcommands of the
symbol, `nav` you can change the current path of the session.  All
subcommands of a symbol `nav` are specified inside the element
`ACTION` - each command on a separate line.

Symbol subcommands `nav`:

 * `push <view>` - enter the specified `VIEW`.  Another level of nesting
   is added to the current session path.
 * `pop [num]` - go to the specified number of nesting levels.  Those
   exclude upper-level sessions from the current path num.  The argument
   numis optional.  Default `num=1`.  If we are already in the root
   `VIEW`, i.e. the current path contains only one level, `pop` will end
   the session and exit klish.
 * `top` - go to the root level of the current session path.  Those exit
   all nested `VIEW`.
 * `replace <view>` - replace the `VIEW`, located at the current nesting
   level with the specified `VIEW`.  The number of nesting levels does
   not increase.  Only the very last component of the path changes.
 * `exit` - end the session and exit klish.

```xml
<ACTION sym="nav">
    pop
    push /view_name1
</ACTION>
```

The example shows how a subcommand can be repeated replaceusing other subcommands.

### Secondary functions

#### Symbol `nop`

Empty command. The symbol does nothing. Always returns the value 0 - "success".

#### Symbol `print`

Prints the text specified in the body of the element `ACTION`.  Line feed
is not displayed at the end of the text.

```xml
<ACTION sym="print">Text to print</ACTION>
```

#### Symbol `printl`

Prints the text specified in the body of the element `ACTION`. A newline
is added at the end of the text.

```xml
<ACTION sym="printl">Text to print</ACTION>
```

#### Symbol `pwd`

Prints the current session path. Needed mainly for debugging.

#### Symbol `prompt`

The symbol prompthelps to form the prompt text for the operator.  The
body of the element `ACTION` specifies the prompt text, which may
contain substitutions of the form `%s`, where "s" is a printable
character.  Instead of such a construction, a certain string is
substituted in the text. List of implemented substitutions:

 * `%%` - the `%` character itself.
 * `%h` - hostname.
 * `%u` - the name of the current user.

```xml
<ACTION sym="prompt">%u@%h&gt; </ACTION>
```

## Plugin "script"

The "script" plugin contains only one symbol `script` and it is used to
execute scripts.  The script is contained in the body of the element
`ACTION`.  The script can be written in different scripting languages.
By default, the script is considered to be written for a POSIX shell
interpreter and run with `/bin/sh`.  To select a different interpreter,
a "shebang" is used.  The shebang is the text of the form
`#!/path/to/binary`, found on the very first line of the script.  Text
`/path/to/binary` is the path where the script interpreter is located.

The plugin `script` is included in the source code of the klish project,
and you can connect the plugin as follows:

```xml
<PLUGIN name="script"/>
```

The name of the command to be executed and the parameter values ​​are
passed to the script using environment variables.  The following
environment variables are supported:

 * `KLISH_COMMAND` - the name of the command to be executed.
 * `KLISH_PARAM_<foo>` - the value of the parameter named `<foo>`.
 * `KLISH_PARAM_<foo>_<index>` - one parameter can have many values ​​if
   an attribute `max` is set for it and the value of this attribute is
   greater than one.  Then the values ​​can be obtained by index.

Examples:

```xml
<COMMAND name="ls" help="List path">
	<PARAM name="path" ptype="/STRING" help="Path"/>
	<ACTION sym="script">
	echo "$KLISH_COMMAND"
	ls "$KLISH_PARAM_path"
	</ACTION>
</COMMAND>

<COMMAND name="pytest" help="Test for Python script">
	<ACTION sym="script">#!/usr/bin/python
	import os
	print('ENV', os.getenv("KLISH_COMMAND"))
	</ACTION>
</COMMAND>
```

The "ls" command uses the shell interpreter and displays a list of files
at the path specified in the "path" parameter. Please note that using
the shell may be unsafe, due to the potential for the operator to inject
arbitrary text into the script and, accordingly, execute an arbitrary
command. Using the shell is available but not recommended. It is very
difficult to write a secure shell script.

The "pytest" command executes a Python script.  Notice where the shebang
is defined.  The first line of the script is the line immediately
following the `ACTION`.  The line following the line in which it is
declared `ACTION` is already considered the second one and it is
impossible to define a shebang in it.


## Plugin "lua"

The "lua" plugin contains only one character luaand is used to execute
scripts in the "Lua" language. The script is contained in the body of
the element `ACTION`.  Unlike the symbol `script` from the "script"
plugin, the symbol `lua` does not call an external interpreter to
execute scripts, but uses internal mechanisms to do so.

The plugin `lua` is included in the source code of the klish project,
and you can connect the plugin as follows:

```xml
<PLUGIN name="lua"/>
```

The contents of a tag can specify a configuration.


## Configuration Options

Consider the plugin configuration options.

### autostart

```
autostart="/usr/share/lua/klish/autostart.lua"
``` 

When the plugin is initialized, the state of the Lua machine is created
which (after the fork) will be used to call the Lua `ACTION` code.  If
you need to load some libraries, this can be done by specifying an
autostart file. There can only be one parameter.

### package path

```
package.path="/usr/lib/lua/clish/?.lua;/usr/lib/lua/?.lua"
```

Sets the Lua package.path (the paths to search for modules). There can
only be one parameter.

### backtrace

```
backtrace=1
```

Whether to show backtrace on Lua code crashes. 0 or 1.

## API

The following functions are available when executing Lua `ACTION`:

### klish.pars()

Returns information about parameters.  There are two possible uses for
this feature.

```
local pars = klish.pars()
for k, v in ipairs(pars) do
  for i, p in ipairs(pars[v]) do
    print(string.format("%s[%d] = %s", v, i, p))
  end
end
```

Getting information about all parameters.  In this case, the function is
called without arguments and returns a table of all parameters.  The
table contains both a list of names and an array of values ​​for each
name.  The example above shows an iterator over all the parameters,
outputting their values.

In addition, `klish.pars()` can be called to take the values ​​of a
particular parameter, for example:

```
print("int_val = ", klish.pars('int_val')[1])
```

### klish.ppars()

Works exactly the same as klish.ppars(), but only for parent parameters,
if they exist in this context.

### `klish.par()` and `klish.ppar()`

They work in the same way as `klish.pars()`, `klish.ppars()` with
setting a specific parameter, but they return not an array, but a value.
If there are multiple parameters with the same name, the first one will
be returned.

### `klish.path()`

Returns the current path as an array of strings.  For example:

```
print(table.concat(klish.path(), "/"))
```

### klish.context()

Allows you to get some parameters of the command context. It takes as
input the string -- the name of the context parameter:

 * val;
 * cmd;
 * pcmd;
 * id;
 * pid;
 * user.

Without a parameter, returns a table with all available context parameters.
