# Contributing to Speed Limit Display

Welcome! This guide explains how we work together on this project using Git and GitHub.

## Branches

We use two main branches:

| Branch | Purpose | Push directly? |
|--------|---------|----------------|
| `main` | Stable, tested code. This is what gets flashed to devices. | **NO** — use a Pull Request |
| `dev` | Work in progress. New features and fixes go here first. | **NO** — use a Pull Request |

### How it flows

```
Your feature branch → Pull Request → dev → (when ready) → Pull Request → main
```

Think of it like this:
- **`main`** = the "release" version. Always works.
- **`dev`** = the "testing" version. Might have new stuff that's not fully tested yet.
- **Your branch** = your personal workspace. Break whatever you want here.

## How to Make Changes (Step by Step)

### 1. Get the latest code

```bash
git checkout dev
git pull
```

This makes sure you're starting from the most up-to-date version.

### 2. Create a new branch for your work

```bash
git checkout -b feat/your-feature-name
```

**Branch naming:**
| Type | Prefix | Example |
|------|--------|---------|
| New feature | `feat/` | `feat/add-buzzer-alert` |
| Bug fix | `fix/` | `fix/gps-baud-rate` |
| Cleanup/maintenance | `chore/` | `chore/update-readme` |

### 3. Make your changes

Edit the code in Arduino IDE or your preferred editor. Save your files.

### 4. See what you changed

```bash
git status
```

This shows which files you modified.

### 5. Stage and commit your changes

```bash
git add SpeedLimitDisplay/SpeedLimitDisplay.ino
git commit -m "feat: add buzzer alert for overspeed"
```

**Commit message format:** Start with a type, then a short description:
- `feat: add buzzer alert for overspeed`
- `fix: correct GPS baud rate command`
- `chore: update pin assignments in config`

### 6. Push your branch to GitHub

```bash
git push -u origin feat/your-feature-name
```

The `-u` flag is only needed the first time you push a new branch. After that, just `git push`.

### 7. Open a Pull Request

1. Go to the repo on GitHub
2. You'll see a yellow banner saying "your branch was recently pushed"
3. Click **"Compare & pull request"**
4. Write a short description of what you changed and why
5. Set the target branch to **`dev`** (not `main`)
6. Click **"Create pull request"**

### 8. Get it reviewed and merged

The other collaborator reviews your changes. Once approved, click **"Merge pull request"** on GitHub. Then delete the branch (GitHub will offer a button for this).

### 9. Clean up locally

```bash
git checkout dev
git pull
git branch -d feat/your-feature-name
```

## Quick Reference

```bash
# See what branch you're on
git branch

# Switch to an existing branch
git checkout dev

# Create a new branch and switch to it
git checkout -b feat/my-feature

# See what files changed
git status

# Stage a specific file
git add SpeedLimitDisplay/SpeedLimitDisplay.ino

# Commit with a message
git commit -m "feat: description of change"

# Push to GitHub
git push

# Pull the latest changes from GitHub
git pull
```

## Rules

1. **Never push directly to `main` or `dev`** — always use a Pull Request
2. **Always branch from `dev`** for new work (not from `main`)
3. **Test on hardware before merging to `main`** — `dev` is for "probably works", `main` is for "definitely works"
4. **Don't commit `config_secrets.h`** — it's in `.gitignore` for a reason. If git asks you to add it, say no.
5. **Keep commits small and focused** — one feature or fix per branch. Don't bundle unrelated changes.

## Setting Up for the First Time

If you just got added as a collaborator:

```bash
# Clone the repo
git clone https://github.com/phreakhcker/Speed-Limit-Display.git
cd Speed-Limit-Display

# Switch to the dev branch
git checkout dev

# Set up your credentials
cp SpeedLimitDisplay/config_secrets_example.h SpeedLimitDisplay/config_secrets.h
# Edit config_secrets.h with your WiFi and API key
```

You're ready to start working!
