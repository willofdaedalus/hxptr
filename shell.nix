{ pkgs ? import <nixpkgs> {} }:

let
  llvmPkgs = pkgs.llvmPackages_22;
in
pkgs.mkShell {
  # packages required at runtime/development
  buildInputs = with pkgs; [
    # Core build tools
    gnumake
    bear

    # llvm/clang tools (ensures clangd and clang match)
    clang_22
    llvmPkgs.clang-tools
	gdb
  ];

  shellHook = ''
  echo "dev env loaded..."
  clang --version
  '';
  # this ensures that standard c library headers are discoverable
  # by clangd inside this specific environment.
  CPATH = "${pkgs.glibc.dev}/include";
}
