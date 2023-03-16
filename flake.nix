{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs";
  outputs = { self, nixpkgs }:
    let sys = "x86_64-linux";
    in {
      packages.${sys}.default = with nixpkgs.legacyPackages.${sys}; stdenv.mkDerivation {
        name = "deckbd";
        src = ./.;
        buildInputs = [ libevdev glib.dev ];
        nativeBuildInputs = [ pkg-config ];
        makeFlags = [ "PREFIX=$(out)" ];
      };
    };
}
