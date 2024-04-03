# Converting a BGT project to NVGT
Our goal is to make the transission as seemless as possible from BGT to NVGT, but here are some things you should know when porting an existing BGT game:

* Always #include "bgt_compat.nvgt" as this includes a lot of aliases to other functions. Using built-in functions may improve performance so this is more or less a stop-gap to get you up and running quickly.
* When refering to an array's length, pass length as a method call and not as a property.
* stream() is not included in NVGT, so change all stream calls to load() instead.
* load() contains a second optional argument to pass a pack file. set_sound_storage() is no longer used for the moment. However, you can set sound_pool.pack_file to a pack handle to get a similar effect.
* The Angelscript syntax for naming arguments in function calls has changed. func(arg, arg, arg=value); must now become func(arg, arg, arg:value);
* Take care to check any method calls using the tts_voice object as a few methods such as set_voice have changed from their BGT counterparts.
* When splitting a string, matching against \r\n is advised as BGT handles this differently. This will result in not having spurious line breaks at the ends of split text.
* Currently directory_create will not create a sub-level directory if the top-level one does not exist. Example. passing Developer/my game will fail if the Developer directory does not exist.
* The settings object has been crafted as a ghost object. That is, it will not actually load or write any data from the registry. If you use the registry, consider using a data or ini file instead.
* The joystick object is also a ghost object and does not currently function.
* The calendar object has been renamed to datetime.
* There is a type called var in the engine now, so you may need to be careful if your project contains any variables named var.