# 2026-02-04T14:24:33.900955600
import vitis

client = vitis.create_client()
client.set_workspace(path="lab1")

platform = client.create_platform_component(name = "lab1_platform",hw_design = "$COMPONENT_LOCATION/../../lab1_hw/ece315_lab1/lab1_hw_wrapper.xsa",os = "freertos",cpu = "ps7_cortexa9_0",domain_name = "freertos_ps7_cortexa9_0")

platform = client.get_component(name="lab1_platform")
status = platform.build()

comp = client.create_app_component(name="lab1_part1",platform = "$COMPONENT_LOCATION/../lab1_platform/export/lab1_platform/lab1_platform.xpfm",domain = "freertos_ps7_cortexa9_0")

comp = client.get_component(name="lab1_part1")
status = comp.import_files(from_loc="", files=["C:\Users\nedillon\Documents\lab1_src\part1\lab1_part1.c", "C:\Users\nedillon\Documents\lab1_src\part1\pmodkypd.c", "C:\Users\nedillon\Documents\lab1_src\part1\pmodkypd.h"])

status = platform.build()

comp = client.get_component(name="lab1_part1")
comp.build()

domain = platform.get_domain(name="freertos_ps7_cortexa9_0")

status = domain.set_config(option = "os", param = "freertos_tick_rate", value = "1000")

status = domain.regenerate()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

comp = client.create_app_component(name="lab1_part2",platform = "$COMPONENT_LOCATION/../lab1_platform/export/lab1_platform/lab1_platform.xpfm",domain = "freertos_ps7_cortexa9_0")

comp = client.get_component(name="lab1_part2")
status = comp.import_files(from_loc="", files=["C:\Users\nedillon\Documents\lab1_src\part2\pmodkypd.c", "C:\Users\nedillon\Documents\lab1_src\part2\pmodkypd.h", "C:\Users\nedillon\Documents\lab1_src\part2\rgb_led.h"])

client.delete_component(name="lab1_part2")

comp = client.create_app_component(name="lab1_part2",platform = "$COMPONENT_LOCATION/../lab1_platform/export/lab1_platform/lab1_platform.xpfm",domain = "freertos_ps7_cortexa9_0")

status = comp.import_files(from_loc="", files=["C:\Users\nedillon\Documents\lab1_src\part2\lab1_part2.c", "C:\Users\nedillon\Documents\lab1_src\part2\pmodkypd.c", "C:\Users\nedillon\Documents\lab1_src\part2\pmodkypd.h", "C:\Users\nedillon\Documents\lab1_src\part2\rgb_led.h"])

status = platform.build()

comp = client.get_component(name="lab1_part2")
comp.build()

status = platform.build()

comp = client.get_component(name="lab1_part1")
comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp = client.get_component(name="lab1_part2")
comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp = client.get_component(name="lab1_part1")
comp.build()

status = platform.build()

comp = client.get_component(name="lab1_part2")
comp.build()

vitis.dispose()

