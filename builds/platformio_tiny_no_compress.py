# PigTeam : envoi NON compresse vers la flash (cartes Tiny N8R8).
# Sur ces cartes, l'ecriture compressee d'esptool ("write-flash -z") fait planter
# uploadfs ("The chip stopped responding") ; l'envoi non compresse passe.
# "upload_flags" ne convient pas : PlatformIO le place AVANT la sous-commande write-flash,
# alors que --no-compress est une option de write-flash. On remplace donc "-z" ici.
Import("env")  # noqa: F821

flags = list(env.get("UPLOADERFLAGS", []))  # noqa: F821
if "-z" in flags:
    flags[flags.index("-z")] = "--no-compress"
    env.Replace(UPLOADERFLAGS=flags)  # noqa: F821
    print("[PigTeam] esptool : envoi non compresse (--no-compress)")
