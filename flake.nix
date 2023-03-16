{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable-small";

  outputs = { self, nixpkgs }:
    let sys = "x86_64-linux";
    in {
      packages.${sys}.default = with nixpkgs.legacyPackages.${sys}; stdenv.mkDerivation {
        name = "deckbd";
        src = ./.;
        buildInputs = [ libevdev ];
        nativeBuildInputs = [ pkg-config ];
        makeFlags = [ "PREFIX=$(out)" ];
      };
    };
}
