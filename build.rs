fn main() {
    let rc = "src/plugin/NppVim.rc";

    println!("cargo:rerun-if-changed={}", rc);

    let macros: &[&str] = &[];

    embed_resource::compile(rc, macros);
}
