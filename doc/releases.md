# Releases  

Push a version tag to publish a GitHub Release with binaries:  

```bash
git tag v1.0.0
git push origin v1.0.0
```

The **Release** workflow (`.github/workflows/release.yml`) builds, tests, and attaches:  

- `comettextel-<version>-windows-x64.zip`  
- `comettextel-<version>-linux-x64.tar.gz`  
- `comettextel-<version>-c-sdk-windows-x64.zip`  
- `comettextel-<version>-c-sdk-linux-x64.tar.gz`  
