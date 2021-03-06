# Yostat

Generate visible resource usage trees for yosys designs using the output JSON.
Each output row shows the logic used by that row and all rows below it,
allowing you to drill down on what parts of the design are using your logic
resources.
Columns can be dragged to rearrange and sorted to focus on the primitives you
care most about.

![Screenshot](/img/yostat.png?raw=true)
Example output for the
[Hackaday Supercon](https://github.com/Spritetm/hadbadge2019_fpgasoc)
badge gateware

### Usage

To compile, ensure you have the wx development headers installed
(`libwxgtk3.0-gtk3-dev`), then build using CMake:

    git clone git@github.com:rschlaikjer/yostat.git yostat
    cd yostat
    git submodule init && git submodule update
    mkdir build
    pushd build
    cmake ..
    make -j$(nproc)
    popd

To get the best results, generate your synthesized yosys output using
`-noflatten` to preserve module structure. An example command for the
`soc_versa5g` example in the
[project trellis examples](https://github.com/SymbiFlow/prjtrellis/tree/master/examples/soc_versa5g)
would be:

    yosys -p "synth_ecp5 -json soc_noflatten.json -top top -abc9 -noflatten" top.v pll.v attosoc.v picorv32.v simpleuart.v
    yostat soc_noflatten.json
