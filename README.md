# UniSequence

![GitHub Workflow Status](https://img.shields.io/github/actions/workflow/status/Ericple/VATPRC-UniSequence/msbuild.yml?style=flat-square)
![GitHub language count](https://img.shields.io/github/languages/count/Ericple/VATPRC-UniSequence?style=flat-square)
![GitHub](https://img.shields.io/github/license/Ericple/VATPRC-UniSequence?style=flat-square)
![GitHub release (latest by date including pre-releases)](https://img.shields.io/github/v/release/Ericple/VATPRC-UniSequence?display_name=tag&include_prereleases&style=flat-square)

# INTRODUCTION

## What

VATPRC-UniSequence is a sequence helper for controllers to manage their aircraft list, and arrange their order.

## How

VATPRC-UniSequence will manage all of your tags, you just need to set their status, and the sorting will be automatically managed by the plugin. Of course, you can sort it yourself if you need to.

## Why

VATPRC-UniSequence aims to reduce the waste of frequency resources and controller time caused by crews frequently asking for their sequence number. So that both sides can have a better experience in flight events.

## Setting up your plugin

Setting up this plugin is very simple, you just need to follow the guide in the following two steps.

### 1. Load your plugin

![image](https://github.com/Ericple/VATPRC-UniSequence/assets/13230558/250f49e6-baf2-47ec-ad73-87c5e0d5d3fc)

Go to your OTHER SET menu, find "Plug-ins ..."

![image](https://github.com/Ericple/VATPRC-UniSequence/assets/13230558/f9d17bbc-5379-411d-9c73-82a4c701c541)

In the Plug-ins Dialog, click the "Load" button in the upper right corner, then locate the.dll file you just downloaded and load it into EuroScope.

### 2. Do something with Departure List

Press this little button marked with "S"

![image](https://github.com/Ericple/VATPRC-UniSequence/assets/13230558/237d624b-7d96-4742-ba6d-379b1647530f)

Add an item like shown in the image below

![image](https://github.com/Ericple/VATPRC-UniSequence/assets/13230558/52e30d3c-d519-42da-b03e-1be629ab9d95)

# HOW TO USE

> **Warning**: Changes are not real time and take some time to synchronize with the server, so don't rush to see results after you make changes. If nothing changes within 5 seconds, check the notification in the "UniSequence" in the bottom left corner to see the cause of the error. Usually, the inability to change state has something to do with the network, and trying again will fix the problem.


## Change the status of crew

If you have correctly configured the plugin according to the above guidelines, you can select from the list the state you want to assign to the unit by left-clicking in the SEQ/STS field.

![image](https://github.com/Ericple/VATPRC-UniSequence/assets/13230558/3932fa8a-de12-4325-bcd9-db7e8035a7e4)

## Reorder the crews

Right-clicking in the SEQ/STS field will allow a string value to be entered. This value allows <callsign> / -1.
Entering a callsign will move the order of the group you are currently operating to the front of the group corresponding to the callsign you entered. If you type "-1", then this action will move the unit to the top of the unit of the same state.

![image](https://github.com/Ericple/VATPRC-UniSequence/assets/13230558/6ac55af7-2b0c-4bfa-83ce-24031af758cc)

This action will move the sort (06) of CES6938 to the front of CSH9106. After successful operation, the SEQ/STS column of CSH9106 will become "06+PUSH" and CES6938 will become "05+PUSH".👇👇👇

![F2QF1)HCEVX8W~L@GQVS$~B](https://github.com/Ericple/VATPRC-UniSequence/assets/13230558/5777cbad-200b-4e69-ab53-a0242ff3c51e)
