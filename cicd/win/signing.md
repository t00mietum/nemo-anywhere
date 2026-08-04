# Windows code signing

Two paths exist. The project uses SignPath Foundation for released binaries; the local path is kept for anyone who has a token/store certificate.

## A. SignPath Foundation (release CI) - the chosen path

Free OV code signing for open-source projects, done in the cloud. The private key lives in SignPath's HSM; artifacts are signed by submitting them from CI. Two things to know up front:

- It only signs artifacts built by a verifiable CI build, so the released exe is built and signed in `.github/workflows/release-win.yml`, not locally. Local `cicd-win.ps1` stays for dev and dogfooding.
- The signed publisher shown in Windows (UAC / SmartScreen / file Properties) is **SignPath Foundation**, not t00mietum or Nemo Anywhere.

### Prerequisites before applying

- OSI-approved license, no commercial dual-licensing. GPL-2.0-only qualifies.
- Actively maintained, and **already released in the form to be signed**. SignPath wants a real release to point at, so cut the first `v<version>` release before applying.
- Functionality described on the download/README page.

### One-time setup

1. Apply for a SignPath Foundation account at signpath.org and create an organization for the project.
2. Create a project (slug `nemo-anywhere`), an artifact configuration for the single exe, and a signing policy (e.g. `release-signing`).
3. Connect this GitHub repository as the trusted build system, so SignPath can verify each submission came from `release-win.yml` on a tag build.
4. Generate an API token for CI submissions.
5. In the repo, set:
	- Secret `SIGNPATH_API_TOKEN` - the API token.
	- Variable `SIGNPATH_ORG_ID` - the SignPath organization id.
	- Variable `SIGNPATH_POLICY_SLUG` - the signing policy slug (e.g. `release-signing`).

Until `SIGNPATH_API_TOKEN` is set, the workflow still builds and attaches the unsigned exe - the sign step just skips. Once set, tag a release (`v<version>`) and the attached exe is signed.

### Follow-ups (not done yet)

- Sign the release `.zip` contents and, once it exists, the installer - the workflow signs only the single portable exe today.

## B. Local signtool (token / store certificate)

For a certificate you hold yourself (Certum OSS, Azure Trusted Signing, or a commercial EV cert), `cicd-win.ps1` stage 5 signs the packed exe locally via `signtool`, driven entirely by env vars - nothing secret is stored in the repo:

- `NEMO_SIGN_THUMBPRINT` - SHA-1 thumbprint of an installed certificate (store or hardware token). Preferred.
- `NEMO_SIGN_PFX` (+ `NEMO_SIGN_PFX_PASSWORD`) - a `.pfx` on disk (testing / self-signed).
- `NEMO_SIGN_TS_URL` - RFC-3161 timestamp URL (defaults to a public TSA).
- `NEMO_SIGNTOOL` - explicit `signtool.exe` path (else PATH, then the newest Windows SDK build).

Unconfigured, signing is a no-op with a note, so the unsigned dev flow is never blocked. `-NoSign` skips it even when configured. This path signs with your own identity as the publisher, and needs no CI.
