# Windows Runner Diagnostics

This repository runs a short Windows Server 2022 GitHub-hosted runner diagnostic.

## Run it

1. Open the **Actions** tab.
2. Select **Windows Runner Diagnostics**.
3. Select **Run workflow**.

The job checks basic Windows environment details and essential tools, then uploads
`windows-report.json` as the `windows-runner-report` artifact.

GitHub-hosted runners are temporary CI jobs, not persistent cloud desktops or
remote-access servers. This workflow does not expose RDP or create network tunnels.
