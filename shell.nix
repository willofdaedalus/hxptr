{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  # packages required at runtime/development
  buildInputs = with pkgs; [
    # Core build tools
    gnumake
    bear

    # llvm/clang tools (ensures clangd and clang match)
    clang-tools
	gdb
  ];

  # this ensures that standard c library headers are discoverable
  # by clangd inside this specific environment.
  CPATH = "${pkgs.glibc.dev}/include";
}
