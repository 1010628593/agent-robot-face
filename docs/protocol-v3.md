# USB v3 Usage Dashboard contract (implementation target)

Paired firmware/Bridge upgrade; envelope v=3, existing hello min/max3. All v2 catalog/focus/stats/action/selection/ACK semantics remain except version. New usage is independent from face selection. EntireJSON<=8192bytes, <=640parsernodes, nesting<=12, LF framing, oneTXowner.

## Usage browsing

Device stores browsing subject (`current|all|codex|cursor|hermes|workbuddy`), period (`today|7d|30d`), page(integer0..33), usage_rev. Default current/today/page0. Initial host usage installs rev1. Browsing does notcallselect. Mac readonlyqueriesdo notmutatedevicebrowse.

Device `usage_request` body exactly `{request_id:<32hex>,expected_usage_rev:<int>,subject:<enum>,period:<enum>,page:<int>}`. Hostdedupsrequestwithinlink; compareexpectedrevthenupdatebrowseincrementrev, sendusage_ack followedusage. PendingrequestnoUIcommitsuccessuntilACK. Rejectedrequestkeepcurrentview. Host `usage_ack` body exactly `{request_id,status:"accepted"|"rejected",reason:"ok"|"conflict",usage_rev,subject,period,page}`. LateACKcannotrollrevisionback.

Host `usage` body exactly `{usage_rev,subject,period,page,data_rev,host_now_ms,as_of_ms,stale,status,summary,agents,quotas,quota_total,models,model_total,history}`. host_now_ms required exactint0..9007199254740991 is projection clock for reset countdown; as_of_ms nullable exactint is oldest token observation, stale bool, status`ready|starting|error|unavailable`, allothersbounded. Current subject resolved atsendtime to faceagent, payloadsummary/agentsincludesitsactualid; changingfaceagentwhilecurrent keepsviewrev butdata_rev/agentidentitynewprojection. Subjectall returnsall4agents.

`summary` exactly `{agent_id:"all"|<4agent>,total,input,output,cache_read,cache_write,cost_micros,cost_currency,cost_coverage,cost_source,coverage}`. Integersnullable0..9007199254740991; cost_currency is ISO3 or null; cost_micros is actualfee in thatcurrency times1e6. Mixedcurrencies returnnullfee/nullcurrency andpartialcoverage, fullcurrencylistavailableMacAPI. coverage`complete|partial|unknown`;cost_coverage sameenum;cost_sourceprintableASCII<=40 nativeallowlistedcodeor"multiple_native_sources" or empty.

`agents` exactlyfourrows (ordercodex,cursor,hermes,workbuddy), each `{id,total,used_pct,available}`. totalnullableinteger,used_pctnullorfinite0..100,availableboolindicatesrealusageorquotaobserved. Overviewcolors basedusedpct. Representativequota:prefer Codex primary product bucket over separate model buckets, then first available window with a non-null used percentage and nearest reset (noresetlast), stabletieID; accountsharingnotduplicatedintotal.

`quotas` pageupto3rows each `{id,agent_id,label,used_pct,reset_ms,stale,availability}`. id32hex,agent_idenum4,labelprintableASCII<=24 (UIlocalizesknown5hour/week/month/credits),used_pctnullablefinite0..100,reset_msnullableint,stalebool,availability`available|unavailable|needs_auth|error`;quota_total0..32.

`models` pageupto3rows each `{label,total}` labelprintableASCII<=32 (sanitizeunknownifnonASCII);model_total0..100 (top100 models; fullcountinMacAPI). `history` <=30 rows each `{day:"YYYY-MM-DD",total:<nullableinteger>}`. Noimplicitzero, no syntheticcurve. Packetbudgetboundedbyconstruction; testmaxarraysagainst8192/640beforehardware.

Hierarchylocal: overviewtoolsfourgrid/dimensionsfourgrid -> lists3rows -> detailgauge/token/cost/cache. Taprouteslocal; horizontal swipepages, nesteddownpopsonelevel beforeglobalnavigationownscontact; rootdowndismissesFace. Geometrycomputedfromdeviceabsolute466coords currentpanelcardoffsetpreserved. NoLVGLwritesserialthread.

Transport implementation (3.2.3): Bridge retains one writer and a 32KiB bounded TX byte queue, flushed nonblocking in chunks up to 512 bytes. Partial writes retain their unsent tail. A transient zero-byte write does not reopen the serial port. A usage request sends ACK then usage only; it does not resend the unrelated full snapshot. Device `@heap` diagnostics are separate from `@bot` frames and cannot mutate business or usage state.

Firmware 3.2.7 uses an 8192-byte single-writer/single-reader FreeRTOS stream buffer between USB RX and the UI thread. The UI consumes up to 2048 bytes per poll. Capacity is measured in bytes rather than USB read fragments; small reads no longer each consume one fixed queue slot. An actual overflow still invalidates the partial frame and starts a fresh handshake.
